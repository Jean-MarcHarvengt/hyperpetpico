#include "pico/cyw43_arch.h"
#include "dhcpserver.h"
#include "network.h"

/********************************
 * Initialization
********************************/ 
uint32_t wifi_init(void)
{
  uint32_t ip = 0;
  if (cyw43_arch_init()) {
      printf("failed to initialise\n");
      return ip;
  }
#ifdef WIFI_AP
  cyw43_arch_enable_ap_mode("hyperpetpico", "picopet123", CYW43_AUTH_WPA2_MIXED_PSK );
  //CYW43_AUTH_WPA2_MIXED_PSK
  //CYW43_AUTH_OPEN
  printf("Connecting to WiFi...\n");
  struct netif *netif = netif_default;
  ip4_addr_t addr = { .addr = 0x017BA8C0 }, mask = { .addr = 0x00FFFFFF };
  ip = 0x017BA8C0; // 192.168.123.1
  printf("IP Address: %lu.%lu.%lu.%lu\n", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, ip >> 24);
  netif_set_addr(netif, &addr, &mask, &addr);
  //set_secondary_ip_address(0x006433c6);

  // Start the dhcp server
  static dhcp_server_t dhcp_server;
  dhcp_server_init(&dhcp_server, &netif->ip_addr, &netif->netmask, "picodomain");
#else
  cyw43_arch_enable_sta_mode();
  // this seems to be the best be can do using the predefined `cyw43_pm_value` macro:
  // cyw43_wifi_pm(&cyw43_state, CYW43_PERFORMANCE_PM);
  // however it doesn't use the `CYW43_NO_POWERSAVE_MODE` value, so we do this instead:
  cyw43_wifi_pm(&cyw43_state, cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 20, 1, 1, 1));
  printf("Connecting to WiFi...\n");

  int retry = 10;
  while (retry-- > 0) { 
    if (cyw43_arch_wifi_connect_timeout_ms("yourap", "yourpasswd", CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("failed to connect, retrying %d...\n",retry);
        sleep_ms(500);
    } else 
    {
        printf("Connected.\n");
        extern cyw43_t cyw43_state;
        auto ip_addr = cyw43_state.netif[CYW43_ITF_STA].ip_addr.addr;
        printf("IP Address: %lu.%lu.%lu.%lu\n", ip_addr & 0xFF, (ip_addr >> 8) & 0xFF, (ip_addr >> 16) & 0xFF, ip_addr >> 24);
        ip = ip_addr;
        break;
    }
  }  
#endif

  return ip; 
}

