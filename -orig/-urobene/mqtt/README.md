21.6.2023: zaklad zobraty z mqtt komponentu z dev verzie po 2023.5.5
21.6.2023: upravene can_proceed, aby presiel setup aj pri vypnutom wifi    bool MQTTClientComponent::can_proceed() {return true;} //{ return this->is_connected(); }
21.6.2023: upravene start_dnslookup_, aby sa nepokusal o connect pri vypnutom wifi
  //skontrolovat pripojenie k wifi, ak je down nepokusat sa dalej o connect
  if (!network::is_connected()) {
    this->state_ = MQTT_CLIENT_DISCONNECTED;
    return;
  }
20.11.2023 upravene na novu ver 2023.11.2
20.11.2023 mqtt_client.cpp ostali zaremovane 2x predosle upravy, mozno druha bude este potrebna

14.10.2024 upravene na novu ver 2024.9.2 (upraveny len mqtt_client.cpp)