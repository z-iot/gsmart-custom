#include "ConfigDef.h"
#include "esphome/components/json/json_util.h"

ConfigDef::ConfigDef(std::shared_ptr<AsyncWebServer> server)
{
  server->on(ConfigDef_PATH, HTTP_GET, std::bind(&ConfigDef::get, this, std::placeholders::_1));
  server->on(ConfigDef_PATH, HTTP_POST, std::bind(&ConfigDef::post, this, std::placeholders::_1));
}

void ConfigDef::get(AsyncWebServerRequest *request)
{

  // std::string data =  esphome::json::build_json([](JsonObject root) {

  // JsonArray permissions = root["permissions"].to<JsonArray>();

  // JsonArray classes = root["classes"].to<JsonArray>();
  // JsonObject c;

  // c = classes.add<JsonObject>();
  // c["title"] = "Network";
  // c["code"] = "network";

  // c = classes.add<JsonObject>();
  // c["title"] = "Region";
  // c["code"] = "region";

  // c = classes.add<JsonObject>();
  // c["title"] = "Actuator";
  // c["code"] = "actuator";

  // c = classes.add<JsonObject>();
  // c["title"] = "Emitter";
  // c["code"] = "emitter";

  // c = classes.add<JsonObject>();
  // c["title"] = "Security";
  // c["code"] = "security";

  // c = classes.add<JsonObject>();
  // c["title"] = "Setup";
  // c["code"] = "setup";

  // JsonArray items = root["items"].to<JsonArray>();
  // JsonObject i;

  // i = items.add<JsonObject>();
  // i["title"] = "wifi login";
  // i["code"] = "wifi_login";
  // i["class"] = "network";
  // i["permission"] = "true";
  // i["kind"] = "string";

  // i = items.add<JsonObject>();
  // i["title"] = "ap login";
  // i["code"] = "ap_login";
  // i["class"] = "network";
  // i["permission"] = "true";
  // i["kind"] = "string";

  // i = items.add<JsonObject>();
  // i["title"] = "wifi ap enable";
  // i["code"] = "wifi_ap_enable";
  // i["class"] = "network";
  // i["permission"] = "true";
  // i["kind"] = "boolean";

  // i = items.add<JsonObject>();
  // i["title"] = "timeZone";
  // i["code"] = "timeZone";
  // i["class"] = "network";
  // i["permission"] = "true";
  // i["kind"] = "string";

  // i = items.add<JsonObject>();
  // i["title"] = "group num";
  // i["code"] = "group_num";
  // i["class"] = "region";
  // i["permission"] = "true";
  // i["kind"] = "number";

  // i = items.add<JsonObject>();
  // i["title"] = "group name";
  // i["code"] = "group_name";
  // i["class"] = "region";
  // i["permission"] = "true";
  // i["kind"] = "string";

  // std::string items = '{"items":[{"title":"wifi login","code":"wifi_login","class":"network","permission":"true","props":{"password":true},"kind":"configItemKind.string"},{"title":"ap login","code":"ap_login","class":"network","permission":"true","props":{"password":true},"kind":"configItemKind.string"},{"title":"wifi ap enable","code":"wifi_ap_enable","class":"network","permission":"true","kind":"configItemKind.boolean"},{"title":"timeZone","code":"timeZone","class":"network","permission":"true","kind":"configItemKind.string"},{"title":"group num","code":"group_num","class":"region","permission":"true","kind":"configItemKind.number"},{"title":"master serial","code":"master_serial","class":"region","permission":"true","kind":"configItemKind.string"},{"title":"members","code":"members","class":"region","permission":"true","kind":"configItemKind.string"},{"title":"lock","code":"lock","class":"actuator","permission":"true","kind":"configItemKind.boolean"},{"title":"btn delays","code":"btn_delays","class":"actuator","permission":"true","kind":"configItemKind.number"},{"title":"pincode","code":"pincode","class":"actuator","permission":"true","props":{"password":true},"kind":"configItemKind.string"},{"title":"lock","code":"lock","class":"emitter","permission":"true","kind":"configItemKind.boolean"},{"title":"sound","code":"sound","class":"emitter","permission":"true","props":{"options":["sound1","sound2","sound3"]},"kind":"configItemKind.select"},{"title":"Guest Password","code":"guest_pass","class":"security","permission":"true","props":{"password":true},"kind":"configItemKind.string"},{"title":"Guest Emial","code":"guest_email","class":"security","permission":"true","kind":"configItemKind.string"},{"title":"Guest Lock","code":"guest_lock","class":"security","permission":"true","kind":"configItemKind.boolean"},{"title":"Guest PinCode","code":"guest_pinCode","class":"security","permission":"true","props":{"password":true},"kind":"configItemKind.string"},{"title":"User Password","code":"user_pass","class":"security","permission":"true","props":{"password":true},"kind":"configItemKind.string"},{"title":"User Emial","code":"user_email","class":"security","permission":"true","kind":"configItemKind.string"},{"title":"User Lock","code":"user_lock","class":"security","permission":"true","kind":"configItemKind.boolean"},{"title":"User PinCode","code":"user_pinCode","class":"security","permission":"true","props":{"password":true},"kind":"configItemKind.string"},{"title":"Admin Password","code":"admin_pass","class":"security","permission":"true","props":{"password":true},"kind":"configItemKind.string"},{"title":"Admin Emial","code":"admin_email","class":"security","permission":"true","kind":"configItemKind.string"},{"title":"Admin Lock","code":"admin_lock","class":"security","permission":"true","kind":"configItemKind.boolean"},{"title":"Admin PinCode","code":"admin_pinCode","class":"security","permission":"true","props":{"password":true},"kind":"configItemKind.string"},{"title":"Brand","code":"brand","class":"setup","permission":"true","kind":"configItemKind.string"},{"title":"Catalog","code":"catalog","class":"setup","permission":"true","kind":"configItemKind.string"},{"title":"Brand Position","code":"brand_pos","class":"setup","permission":"true","kind":"configItemKind.number"},{"title":"Lamp Count","code":"lampCount","class":"setup","permission":"true","kind":"configItemKind.number"},{"title":"Lamp Power","code":"lampPower","class":"setup","permission":"true","kind":"configItemKind.number"}]}';

  //   items: [
  //     // network
  //     {
  //       title: 'wifi login',
  //       code: 'wifi_login',
  //       class: 'network',
  //       permission: 'true',
  //       props: {
  //         password: true
  //       },
  //       kind: configItemKind.string
  //     },
  //     {
  //       title: 'ap login',
  //       code: 'ap_login',
  //       class: 'network',
  //       permission: 'true',
  //       props: {
  //         password: true
  //       },
  //       kind: configItemKind.string
  //     },
  //     {
  //       title: 'wifi ap enable',
  //       code: 'wifi_ap_enable',
  //       class: 'network',
  //       permission: 'true',
  //       kind: configItemKind.boolean
  //     },
  //     {
  //       title: 'timeZone',
  //       code: 'timeZone',
  //       class: 'network',
  //       permission: 'true',
  //       kind: configItemKind.string
  //     },

  //     // region
  //     {
  //       title: 'group num',
  //       code: 'group_num',
  //       class: 'region',
  //       permission: 'true',
  //       kind: configItemKind.number
  //     },
  //     {
  //       title: 'master serial',
  //       code: 'master_serial',
  //       class: 'region',
  //       permission: 'true',
  //       kind: configItemKind.string
  //     },
  //     {
  //       title: 'members',
  //       code: 'members',
  //       class: 'region',
  //       permission: 'true',
  //       kind: configItemKind.string
  //     },

  //     // actuator
  //     {
  //       title: 'lock',
  //       code: 'lock',
  //       class: 'actuator',
  //       permission: 'true',
  //       kind: configItemKind.boolean
  //     },
  //     {
  //       title: 'btn delays',
  //       code: 'btn_delays',
  //       class: 'actuator',
  //       permission: 'true',
  //       kind: configItemKind.number
  //     },
  //     {
  //       title: 'pincode',
  //       code: 'pincode',
  //       class: 'actuator',
  //       permission: 'true',
  //       props: {
  //         password: true
  //       },
  //       kind: configItemKind.string
  //     },

  //     // emitter
  //     {
  //       title: 'lock',
  //       code: 'lock',
  //       class: 'emitter',
  //       permission: 'true',
  //       kind: configItemKind.boolean
  //     },
  //     {
  //       title: 'sound',
  //       code: 'sound',
  //       class: 'emitter',
  //       permission: 'true',
  //       props: {
  //         options: ['sound1', 'sound2', 'sound3']
  //       },
  //       kind: configItemKind.select
  //     },

  //     // security
  //     {
  //       title: 'Guest Password',
  //       code: 'guest_pass',
  //       class: 'security',
  //       permission: 'true',
  //       props: {
  //         password: true
  //       },
  //       kind: configItemKind.string
  //     },
  //     {
  //       title: 'Guest Emial',
  //       code: 'guest_email',
  //       class: 'security',
  //       permission: 'true',
  //       kind: configItemKind.string
  //     },
  //     {
  //       title: 'Guest Lock',
  //       code: 'guest_lock',
  //       class: 'security',
  //       permission: 'true',
  //       kind: configItemKind.boolean
  //     },
  //     {
  //       title: 'Guest PinCode',
  //       code: 'guest_pinCode',
  //       class: 'security',
  //       permission: 'true',
  //       props: {
  //         password: true
  //       },
  //       kind: configItemKind.string
  //     },
  //     {
  //       title: 'User Password',
  //       code: 'user_pass',
  //       class: 'security',
  //       permission: 'true',
  //       props: {
  //         password: true
  //       },
  //       kind: configItemKind.string
  //     },
  //     {
  //       title: 'User Emial',
  //       code: 'user_email',
  //       class: 'security',
  //       permission: 'true',
  //       kind: configItemKind.string
  //     },
  //     {
  //       title: 'User Lock',
  //       code: 'user_lock',
  //       class: 'security',
  //       permission: 'true',
  //       kind: configItemKind.boolean
  //     },
  //     {
  //       title: 'User PinCode',
  //       code: 'user_pinCode',
  //       class: 'security',
  //       permission: 'true',
  //       props: {
  //         password: true
  //       },
  //       kind: configItemKind.string
  //     },
  //     {
  //       title: 'Admin Password',
  //       code: 'admin_pass',
  //       class: 'security',
  //       permission: 'true',
  //       props: {
  //         password: true
  //       },
  //       kind: configItemKind.string
  //     },
  //     {
  //       title: 'Admin Emial',
  //       code: 'admin_email',
  //       class: 'security',
  //       permission: 'true',
  //       kind: configItemKind.string
  //     },
  //     {
  //       title: 'Admin Lock',
  //       code: 'admin_lock',
  //       class: 'security',
  //       permission: 'true',
  //       kind: configItemKind.boolean
  //     },
  //     {
  //       title: 'Admin PinCode',
  //       code: 'admin_pinCode',
  //       class: 'security',
  //       permission: 'true',
  //       props: {
  //         password: true
  //       },
  //       kind: configItemKind.string
  //     },

  //     // setup
  //     {
  //       title: 'Brand',
  //       code: 'brand',
  //       class: 'setup',
  //       permission: 'true',
  //       kind: configItemKind.string
  //     },
  //     {
  //       title: 'Catalog',
  //       code: 'catalog',
  //       class: 'setup',
  //       permission: 'true',
  //       kind: configItemKind.string
  //     },
  //     {
  //       title: 'Brand Position',
  //       code: 'brand_pos',
  //       class: 'setup',
  //       permission: 'true',
  //       kind: configItemKind.number
  //     },
  //     {
  //       title: 'Lamp Count',
  //       code: 'lampCount',
  //       class: 'setup',
  //       permission: 'true',
  //       kind: configItemKind.number
  //     },
  //     {
  //       title: 'Lamp Power',
  //       code: 'lampPower',
  //       class: 'setup',
  //       permission: 'true',
  //       kind: configItemKind.number
  //     }
  //     // TODO Setup - DateRelease

  //     // TODO Mode
  //   ]
  // }

  // });

  // https://jsontostring.com/
  // {"classes":[{"title":"Actuator","code":"actuator"},{"title":"Emitter","code":"emitter"},{"title":"Security","code":"security"},{"title":"Setup","code":"setup"},{"title":"Network","code":"network"},{"title":"Region","code":"region"}],"items":[{"title":"PIN code","code":"pin_cd","class":"actuator","kind":"string"},{"title":"Locked","code":"lock","class":"actuator","kind":"boolean"},{"title":"Sleep","code":"sleep","class":"actuator","kind":"boolean"},{"title":"Brightnes","code":"bright","class":"actuator","kind":"number","props":{"min":0,"max":100}},{"title":"Dimmable","code":"dim","class":"actuator","kind":"number","props":{"min":0,"max":100}},{"title":"Locked","code":"lock","class":"emitter","kind":"boolean"},{"title":"sound","code":"sound","class":"emitter","props":{"options":["Silence","Low","Std","Max"]},"kind":"select"},{"title":"Guest Password","code":"guest_pass","class":"security","kind":"string"},{"title":"Guest email","code":"guest_email","class":"security","kind":"string"},{"title":"Guest Lock","code":"guest_lock","class":"security","kind":"boolean"},{"title":"Guest PinCode","code":"guest_pinCode","class":"security","kind":"string"},{"title":"User Password","code":"user_pass","class":"security","kind":"string"},{"title":"User email","code":"user_email","class":"security","kind":"string"},{"title":"User Lock","code":"user_lock","class":"security","kind":"boolean"},{"title":"User PinCode","code":"user_pinCode","class":"security","kind":"string"},{"title":"Admin Password","code":"admin_pass","class":"security","kind":"string"},{"title":"Admin email","code":"admin_email","class":"security","kind":"string"},{"title":"Admin Lock","code":"admin_lock","class":"security","kind":"boolean"},{"title":"Admin PinCode","code":"admin_pinCode","class":"security","kind":"string"},{"title":"Catalog","code":"catalog","class":"setup","kind":"string"},{"title":"Batch","code":"batch","class":"setup","kind":"string"},{"title":"Batch Position","code":"batch_pos","class":"setup","kind":"number"},{"title":"Lamp Count","code":"lampCount","class":"setup","kind":"number"},{"title":"Lamp Power","code":"lampPower","class":"setup","kind":"number"},{"title":"Wifi SSID","code":"wifi_ssid","class":"network","kind":"string"},{"title":"Wifi password","code":"wifi_pass","class":"network","kind":"string"},{"title":"AP enable","code":"ap_enable","class":"network","kind":"boolean"},{"title":"AP password","code":"ap_pass","class":"network","kind":"string"},{"title":"Time zone","code":"time_zone","class":"network","kind":"string"},{"title":"Region number","code":"reg_num","class":"region","kind":"number"},{"title":"Master serial","code":"master_serial","class":"region","kind":"string"},{"title":"Members","code":"members","class":"region","kind":"string"}]}
  std::string items = "{\"classes\":[{\"title\":\"Actuator\",\"code\":\"actuator\"},{\"title\":\"Emitter\",\"code\":\"emitter\"},{\"title\":\"Security\",\"code\":\"security\"},{\"title\":\"Setup\",\"code\":\"setup\"},{\"title\":\"Network\",\"code\":\"network\"},{\"title\":\"Region\",\"code\":\"region\"}],\"items\":[{\"title\":\"PINcode\",\"code\":\"pin_cd\",\"class\":\"actuator\",\"kind\":\"string\"},{\"title\":\"Locked\",\"code\":\"lock\",\"class\":\"actuator\",\"kind\":\"boolean\"},{\"title\":\"Sleep\",\"code\":\"sleep\",\"class\":\"actuator\",\"kind\":\"boolean\"},{\"title\":\"Brightnes\",\"code\":\"bright\",\"class\":\"actuator\",\"kind\":\"number\",\"props\":{\"min\":0,\"max\":100}},{\"title\":\"Dimmable\",\"code\":\"dim\",\"class\":\"actuator\",\"kind\":\"number\",\"props\":{\"min\":0,\"max\":100}},{\"title\":\"Locked\",\"code\":\"lock\",\"class\":\"emitter\",\"kind\":\"boolean\"},{\"title\":\"sound\",\"code\":\"sound\",\"class\":\"emitter\",\"props\":{\"options\":[\"Silence\",\"Low\",\"Std\",\"Max\"]},\"kind\":\"select\"},{\"title\":\"GuestPassword\",\"code\":\"guest_pass\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"Guestemail\",\"code\":\"guest_email\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"GuestLock\",\"code\":\"guest_lock\",\"class\":\"security\",\"kind\":\"boolean\"},{\"title\":\"GuestPinCode\",\"code\":\"guest_pinCode\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"UserPassword\",\"code\":\"user_pass\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"Useremail\",\"code\":\"user_email\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"UserLock\",\"code\":\"user_lock\",\"class\":\"security\",\"kind\":\"boolean\"},{\"title\":\"UserPinCode\",\"code\":\"user_pinCode\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"AdminPassword\",\"code\":\"admin_pass\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"Adminemail\",\"code\":\"admin_email\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"AdminLock\",\"code\":\"admin_lock\",\"class\":\"security\",\"kind\":\"boolean\"},{\"title\":\"AdminPinCode\",\"code\":\"admin_pinCode\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"Catalog\",\"code\":\"catalog\",\"class\":\"setup\",\"kind\":\"string\"},{\"title\":\"Batch\",\"code\":\"batch\",\"class\":\"setup\",\"kind\":\"string\"},{\"title\":\"BatchPosition\",\"code\":\"batch_pos\",\"class\":\"setup\",\"kind\":\"number\"},{\"title\":\"LampCount\",\"code\":\"lampCount\",\"class\":\"setup\",\"kind\":\"number\"},{\"title\":\"LampPower\",\"code\":\"lampPower\",\"class\":\"setup\",\"kind\":\"number\"},{\"title\":\"WifiSSID\",\"code\":\"wifi_ssid\",\"class\":\"network\",\"kind\":\"string\"},{\"title\":\"Wifipassword\",\"code\":\"wifi_pass\",\"class\":\"network\",\"kind\":\"string\"},{\"title\":\"APenable\",\"code\":\"ap_enable\",\"class\":\"network\",\"kind\":\"boolean\"},{\"title\":\"APpassword\",\"code\":\"ap_pass\",\"class\":\"network\",\"kind\":\"string\"},{\"title\":\"Timezone\",\"code\":\"time_zone\",\"class\":\"network\",\"kind\":\"string\"},{\"title\":\"Regionnumber\",\"code\":\"reg_num\",\"class\":\"region\",\"kind\":\"number\"},{\"title\":\"Masterserial\",\"code\":\"master_serial\",\"class\":\"region\",\"kind\":\"string\"},{\"title\":\"Members\",\"code\":\"members\",\"class\":\"region\",\"kind\":\"string\"}]}";

  request->send(200, "text/json", items.c_str());
}

void ConfigDef::post(AsyncWebServerRequest *request)
{

  std::string data = esphome::json::build_json([](JsonObject root)
                                               { root["xxx"] = "XXXX"; });

  request->send(200, "text/json", data.c_str());
}