#pragma once

// Portable journal engine; the firmware and power-cut tests use the same code.
// ACK cursors stay in RAM. Only complete acknowledged segments are erased, so
// reconnect/reboot can resend a short prefix without writing flash per ACK.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace gsmart_history {
// PANEL has a 64 KiB filesystem despite sharing ESP32 with 16 MiB emitters.
inline constexpr size_t PANEL_SLOTS=6;
enum Kind : uint8_t { BOOT=1, START, STOP, MODE, ACTION, ERROR_START, ERROR_CLEAR, GAP, CLOCK, CONFIG };
#pragma pack(push,1)
struct Record {
  uint32_t crc{0};
  uint64_t boot{0}, wall_ms{0}, uptime_ms{0}, region{0};
  uint32_t sequence{0};
  uint16_t config{0}, code{0};
  uint8_t kind{0}, mode{0}, cause{0}, reserved{0};
  char button[24]{}, origin[16]{}, action[24]{};
};
struct Header { uint32_t magic{0x31484A47}, ordinal{0}; uint64_t generation{0}; uint32_t crc{0}; };
struct Meta { uint32_t magic{0x314D4A47}; uint64_t generation{0}; uint32_t crc{0}; };
#pragma pack(pop)
static_assert(sizeof(Record)<=128,"Journal record exceeds flash budget");
inline uint32_t crc32(const void *data,size_t length) {
  uint32_t crc=~0U;const auto *bytes=static_cast<const uint8_t*>(data);
  while(length--){crc^=*bytes++;for(int b=0;b<8;b++)crc=(crc>>1)^(0xEDB88320U&-(crc&1));}
  return ~crc;
}
inline void seal(Record &r){r.crc=crc32(reinterpret_cast<const uint8_t*>(&r)+4,sizeof(r)-4);}
inline bool valid(const Record &r){return r.kind>=BOOT&&r.kind<=CONFIG&&r.sequence>0&&r.boot!=0&&r.crc==crc32(reinterpret_cast<const uint8_t*>(&r)+4,sizeof(r)-4);}

class Files {
 public:
  virtual ~Files()=default;
  // -1 is absent, -2 is I/O failure. Slot -1 is generation metadata.
  virtual int size(int slot)=0;
  virtual bool read(int slot,size_t offset,void *out,size_t length)=0;
  virtual bool append(int slot,const void *data,size_t length)=0;
  // Create uses write+sync+atomic rename; never replaces an existing live file.
  virtual bool create(int slot,const void *data,size_t length)=0;
  virtual bool truncate(int slot,size_t length)=0;
  virtual bool erase(int slot)=0;
};

template<size_t Slots> class Journal {
 public:
  static constexpr size_t SEGMENT_BYTES=4096;
  static constexpr size_t CAPACITY=(SEGMENT_BYTES-sizeof(Header))/sizeof(Record);
  explicit Journal(Files &files):files_(files){}
  bool begin(uint64_t fresh_generation,uint64_t boot) {
    boot_=boot;sequence_=0;count_=0;healthy_=false;blocked_=false;torn_=false;dropped_=0;
    Meta meta{};const int meta_size=files_.size(-1);
    if(meta_size==-1){
      // Missing metadata alongside data is corruption, never a new generation.
      for(size_t i=0;i<Slots;i++)if(files_.size(i)!=-1)return false;
      meta.generation=fresh_generation;meta.crc=crc32(&meta,offsetof(Meta,crc));
      if(!fresh_generation||!files_.create(-1,&meta,sizeof(meta)))return false;
    }else if(meta_size!=sizeof(meta)||!files_.read(-1,0,&meta,sizeof(meta)))return false;
    if(meta.magic!=0x314D4A47||!meta.generation||meta.crc!=crc32(&meta,offsetof(Meta,crc)))return false;
    generation_=meta.generation;
    for(size_t i=0;i<Slots;i++){
      const int size=files_.size(i);if(size==-1)continue;
      Header h{};
      if(size<int(sizeof(h))||!files_.read(i,0,&h,sizeof(h))||h.magic!=0x31484A47||h.generation!=generation_||h.crc!=crc32(&h,offsetof(Header,crc)))return false;
      if(size>int(SEGMENT_BYTES))return false;
      segments_[count_++]={int(i),h.ordinal,uint16_t((size-sizeof(h))/sizeof(Record)),0};
    }
    std::sort(segments_,segments_+count_,[](const Segment&a,const Segment&b){return a.ordinal<b.ordinal;});
    for(size_t i=0;i<count_;i++){
      auto &s=segments_[i];if(i&&s.ordinal==segments_[i-1].ordinal)return false;
      for(size_t j=0;j<s.records;j++){
        Record r{};if(!read_(s,j,r))return false;
        if(!valid(r)){
          // Only an interrupted final append is repairable automatically.
          // Keep all previous complete records; boot/gap prevents invented time.
          if(i!=count_-1||j+1!=size_t(s.records))return false;
          s.records=j;torn_=true;break;
        }
      }
      const size_t expected=sizeof(Header)+s.records*sizeof(Record);
      if(files_.size(s.slot)!=int(expected)){
        if(i!=count_-1||!files_.truncate(s.slot,expected))return false;
        torn_=true;
      }
    }
    healthy_=boot!=0;
    return healthy_;
  }
  bool push(Record r) {
    if(!healthy_||sequence_>=2147483645U)return false;
    if(!ensure_space_()) {blocked_=true;dropped_++;return false;}
    auto &last=segments_[count_-1];
    // Leave one durable gap record before an unacknowledged full journal. Old
    // records are preserved. Further changes are explicitly unobserved.
    if(last.records==CAPACITY-1&&count_==Slots&&r.kind!=GAP){
      Record gap=r;gap.kind=GAP;gap.code=1;
      if(!write_(gap))return false;
      blocked_=true;dropped_++;return false;
    }
    if(blocked_&&r.kind!=GAP){
      Record gap=r;gap.kind=GAP;gap.code=2;
      if(!write_(gap)||!ensure_space_())return false;
    }
    blocked_=false;
    return write_(r);
  }
  // A batch never crosses a boot boundary. At most four small records are held
  // on a REX; JSON allocation happens only after these bounded reads.
  size_t peek(Record *out,size_t limit) {
    if(!healthy_||!limit)return 0;
    size_t n=0;uint64_t boot=0;
    for(size_t i=0;i<count_;i++)for(size_t j=segments_[i].acked;j<segments_[i].records;j++){
      Record r{};if(!read_(segments_[i],j,r)||!valid(r)){healthy_=false;return 0;}
      if(n&&(boot!=r.boot||n==limit))return n;
      boot=r.boot;out[n++]=r;
      if(n==limit)return n;
    }
    return n;
  }
  bool ack(uint64_t boot,uint32_t sequence) {
    if(!healthy_)return false;
    for(size_t i=0;i<count_;i++){
      auto &s=segments_[i];
      while(s.acked<s.records){
        Record r{};if(!read_(s,s.acked,r)||!valid(r)){healthy_=false;return false;}
        if(r.boot!=boot||r.sequence>sequence)return clean_();
        s.acked++;
      }
    }
    return clean_();
  }
  bool healthy()const{return healthy_;}
  bool blocked()const{return blocked_;}
  bool repaired_tail()const{return torn_;}
  uint64_t generation()const{return generation_;}
  uint64_t boot()const{return boot_;}
  uint32_t dropped()const{return dropped_;}
  size_t pending()const{size_t n=0;for(size_t i=0;i<count_;i++)n+=segments_[i].records-segments_[i].acked;return n;}
 private:
  struct Segment {int slot;uint32_t ordinal;uint16_t records,acked;};
  Files &files_;Segment segments_[Slots]{};size_t count_{0};
  uint64_t generation_{0},boot_{0};uint32_t sequence_{0},dropped_{0};
  bool healthy_{false},blocked_{false},torn_{false};
  bool read_(const Segment&s,size_t index,Record&r){return files_.read(s.slot,sizeof(Header)+index*sizeof(Record),&r,sizeof(r));}
  bool clean_(){
    for(size_t i=0;i<count_;){
      const auto &s=segments_[i];
      if(s.acked==s.records&&(i+1<count_||s.records==CAPACITY)){
        if(!files_.erase(s.slot)){healthy_=false;return false;}
        for(size_t j=i+1;j<count_;j++)segments_[j-1]=segments_[j];
        count_--;
      }else i++;
    }
    return true;
  }
  bool ensure_space_(){
    if(!clean_())return false;
    if(count_&&segments_[count_-1].records<CAPACITY)return true;
    if(count_==Slots)return false;
    int slot=0;for(;slot<int(Slots);slot++){
      bool used=false;for(size_t i=0;i<count_;i++)used|=segments_[i].slot==slot;
      if(!used)break;
    }
    Header h{};h.generation=generation_;h.ordinal=count_?segments_[count_-1].ordinal+1:1;
    h.crc=crc32(&h,offsetof(Header,crc));
    if(!h.ordinal||!files_.create(slot,&h,sizeof(h))){healthy_=false;return false;}
    segments_[count_++]={slot,h.ordinal,0,0};return true;
  }
  bool write_(Record r){
    if(!ensure_space_())return false;
    r.boot=boot_;r.sequence=sequence_+1;seal(r);
    if(!files_.append(segments_[count_-1].slot,&r,sizeof(r))){healthy_=false;return false;}
    sequence_++;segments_[count_-1].records++;return true;
  }
};
} // namespace gsmart_history
