#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/ipv6/multicast/uip-mcast6.h"
#include "dev/leds.h"
#include "sys/compower.h"
#include "sys/etimer.h"
#include <string.h>
#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"
#include "dev/cc2420/cc2420.h"


static uint8_t channel[CHANNELS];					// channel array

static struct uip_udp_conn *sink_conn;					// receiver socket
static struct uip_udp_conn *mcast_conn;					// transmitter socket
static uint16_t count;
static uint32_t seq_id;

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

PROCESS(sink_process, "ROOT, Multicast Sink");
PROCESS(sender_process, "ROOT, Multicast Sender");
AUTOSTART_PROCESSES(&sink_process,&sender_process);

uint32_t node_id, payload, cur_round;					// root ID & current round
uint8_t c, change_channel, my_timeslot;					// flags & timeslot
unsigned long runtime, starttime;					// runtime of the application

/* array of calculated & collected RSSI values (all links between nodes)
*/
signed char rssi_complete[NODE_AMOUNT+1][NODE_AMOUNT+1];

/* timer:  send multicast
*/
static struct etimer et;


/* message to multicast
*/
typedef struct msg{
	uint8_t nodeId;
	signed char rssi_collect[NODE_AMOUNT+1];
} msg_t;


/* print & change radio settings
*/
static void set_radio_default_parameters(uint8_t channel) {
        NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER, TX_POWER);
        NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, channel);
}
static void print_radio_values(void) {
        radio_value_t aux;
        printf("\nRadio parameters:\n");
        NETSTACK_RADIO.get_value(RADIO_PARAM_CHANNEL, &aux);
        printf("   Channel %u", aux);
        NETSTACK_RADIO.get_value(RADIO_CONST_CHANNEL_MIN, &aux);
        printf(" (Min: %u, ", aux);
        NETSTACK_RADIO.get_value(RADIO_CONST_CHANNEL_MAX, &aux);
        printf("Max: %u)\n", aux);
        NETSTACK_RADIO.get_value(RADIO_PARAM_TXPOWER, &aux);
        printf("   Tx Power %3d dBm", aux);
        NETSTACK_RADIO.get_value(RADIO_CONST_TXPOWER_MIN, &aux);
        printf(" (Min: %3d dBm, ", aux);
        NETSTACK_RADIO.get_value(RADIO_CONST_TXPOWER_MAX, &aux);
        printf("Max: %3d dBm)\n", aux);
}


/* receive & process messages
*/
static void tcpip_handler(void) {
	static signed char rss;
  	static signed char rss_val;
  	static signed char rss_offset;
	leds_toggle(LEDS_GREEN);

	if(uip_newdata()) {

    		count++;


                /* save received data
                */
		msg_t received_msg = *(msg_t*) uip_appdata;
		payload = received_msg.nodeId;
		memcpy(&rssi_complete[payload], &received_msg.rssi_collect, sizeof(received_msg.rssi_collect));

                if (COOJA!=0) {
                        PRINTF("In: [%d %d %d %d %d %d %d %d %d %d], from: %u, TTL %u, total %u, round %u, channel %u\n",
                                received_msg.rssi_collect[1],
                                received_msg.rssi_collect[2],
                                received_msg.rssi_collect[3],
                                received_msg.rssi_collect[4],
                                received_msg.rssi_collect[5],
                                received_msg.rssi_collect[6],
                                received_msg.rssi_collect[7],
                                received_msg.rssi_collect[8],
                                received_msg.rssi_collect[9],
                                received_msg.rssi_collect[10],
                                (unsigned int) payload,
                                UIP_IP_BUF->ttl,
                                count,
                                (unsigned int) cur_round,
                                channel[c]);
                }

                /* calculate RSSI of last packet via offset
                */
                rss_val = cc2420_last_rssi;
                rss_offset = -45;
                rss = rss_val + rss_offset;

		rssi_complete[node_id][payload] = rss;		// save RSSI

  	}
	leds_off(LEDS_GREEN);
  	return;
}


/* join multicast group
*/
static uip_ds6_maddr_t * join_mcast_group(void) {
	uip_ipaddr_t addr;
  	uip_ds6_maddr_t *rv;
	leds_toggle(LEDS_RED);

  	/* set global IPv6 addresses & join
	*/
  	uip_ip6addr(&addr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  	uip_ds6_set_addr_iid(&addr, &uip_lladdr);
  	uip_ds6_addr_add(&addr, 0, ADDR_AUTOCONF);
  	uip_ip6addr(&addr, 0xFF01,0,0,0,0,0,0x89,0xABCD);	// set multicast address/group
  	rv = uip_ds6_maddr_add(&addr);

  	if(rv) {
		if (COOJA!=0) {
	    		PRINTF("Joined mcast group ");
	    		PRINT6ADDR(&uip_ds6_maddr_lookup(&addr)->ipaddr);
	    		PRINTF("\n");
		}
		leds_off(LEDS_RED);
  	}
  	return rv;
}


/* receiver thread (continiously receive messages)
*/
PROCESS_THREAD(sink_process, ev, data) {
	PROCESS_BEGIN();

  	if(join_mcast_group() == NULL) {
		if (COOJA!=0) { PRINTF("Failed to join mcast group\n"); }
    		PROCESS_EXIT();
  	}

  	count = 0;

        /* create & bind receiver socket to multicast group
        */
  	sink_conn = udp_new(NULL, UIP_HTONS(0), NULL);
  	udp_bind(sink_conn, UIP_HTONS(MCAST_SINK_UDP_PORT));

        if (COOJA!=0) {
	  	PRINTF("Listening: ");
  		PRINT6ADDR(&sink_conn->ripaddr);
	  	PRINTF(" local/remote port %u/%u\n", UIP_HTONS(sink_conn->lport), UIP_HTONS(sink_conn->rport));
	}

  	while(1) {
    		PROCESS_YIELD();
    		if(ev == tcpip_event) {
                        /* continiously process incoming messages
                        */
      			tcpip_handler();
    		}
  	}
	PROCESS_END();
}


/* send a multicast into group
*/
static void multicast_send(void) {
	leds_toggle(LEDS_BLUE);

        /* prepare message: copy collected RSSI values into temporary message struct
        */
	msg_t msg_send;
	msg_send.nodeId = node_id;

	if (COOJA!=0) {
	  	PRINTF("Sending: ");
  		PRINT6ADDR(&mcast_conn->ripaddr);
	  	PRINTF(" Remote Port %u,", uip_ntohs(mcast_conn->rport));
  		PRINTF(" %lu bytes,", (unsigned long) sizeof(msg_send));
		PRINTF("channel %u\n", (unsigned int) channel[c]);
	}

        /* send prepare message to multicast group members
        */
  	seq_id++;
  	uip_udp_packet_send(mcast_conn, &msg_send, sizeof(msg_send));
	leds_off(LEDS_BLUE);
}


/* create udp connection for multicasting
*/
static void prepare_mcast(void) {
  	uip_ipaddr_t ipaddr;
  	uip_ip6addr(&ipaddr, 0xFF01,0,0,0,0,0,0x89,0xABCD);			// multicast group address
  	mcast_conn = udp_new(&ipaddr, UIP_HTONS(MCAST_SINK_UDP_PORT), NULL);
}


/* set IPv6 addresses
*/
static void set_own_addresses(void) {
  	int i;
  	uint8_t state;
	uip_ipaddr_t ipaddr;

  	uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  	uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  	uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

        /* IPv6 address autoconfiguration
        */
  	if (COOJA!=0) { PRINTF("IPv6 addr:\n"); }
  	for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    		state = uip_ds6_if.addr_list[i].state;
    		if(uip_ds6_if.addr_list[i].isused && (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
			if (COOJA!=0) {
      				PRINTF("  ");
      				PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      				PRINTF("\n");
			}
      			if(state == ADDR_TENTATIVE) {
        			uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
      			}

                        /* create own node id via assigned IPv6 address (only for COOJA simulation!)
                        */
                        if (COOJA!=0) {
				node_id = uip_ds6_if.addr_list[i].ipaddr.u16[7];
       				node_id = node_id <<28;
       				node_id = node_id >>28;
			}
		}
  	}
}


/* sender thread
*/
PROCESS_THREAD(sender_process, ev, data) {
	PROCESS_BEGIN();
	uint8_t i,j;

	starttime = clock_seconds();

        c = 0;					// channel index
	cur_round = 0;				// current round
	change_channel = 1;			// flag: change channel
	node_id = 1;				// root node ID (round initializer ID)

        channel[0] = CHANNEL_A;
        channel[1] = CHANNEL_B;
        channel[2] = CHANNEL_C;
//        channel[3] = CHANNEL_D;         	// add channel

	my_timeslot = (NODE_AMOUNT/2) * (5);				// root timeslot basis calculation

        set_radio_default_parameters(channel[c]);			// set radio power & channel
        if (COOJA!=0) {
		print_radio_values();
		PRINTF("mcast engine: '%s'\n", UIP_MCAST6.name);	// print multicast engine
	}

  	NETSTACK_MAC.off(1);
  	set_own_addresses();					// configure own IPv6 address
  	prepare_mcast();					// prepare sending multicast to specific group
  	etimer_set(&et, (START_DELAY * CLOCK_SECOND));		// wait to start multicasting


  	while(1) {
    		PROCESS_YIELD();
    		if(etimer_expired(&et)) {
                        etimer_reset(&et);
			cur_round = cur_round +1;

      			if(seq_id == ITERATIONS) {
        			etimer_stop(&et);
      			} else {
				if (change_channel==1) {
					/* change channel after START_SWITCH rounds
					*/
					if (cur_round > START_SWITCH) {
						if (c < CHANNELS-1) { c++; }
						else { c=0; }
                                		set_radio_default_parameters(channel[c]);
					}
					/* root multicast (initialize round)
					*/
					multicast_send();
					if (COOJA!=0) {
						PRINTF("id:%u timeslot: %u ms, channel %u\n", (unsigned int) node_id
										, (unsigned int) (1028/my_timeslot)
										, channel[c]);
					}
				}

				/* print all collected RSSI values at the end of one round
				 * note: newline after 3 rounds.
				*/
				i = 0;
                        	j = 0;
                        	for(i=1; i < NODE_AMOUNT+1; i++) {
                                	for(j = 1 ; j < NODE_AMOUNT+1; j++) {
                                        	if (i!=j) {
                                                	PRINTF("%d ", rssi_complete[i][j]);
                                        	}
                                	}
                        	}
                        	if ((cur_round > START_SWITCH) && (cur_round%CHANNELS == 0)) {
                                	runtime = clock_seconds();
                        	        runtime = runtime - starttime;
                                	PRINTF("%lu\n", runtime);
                        	}

                                /* calculate sending timeslot (17-26ms accuracy) & set timer
                                */
        			etimer_set(&et, ((CLOCK_SECOND/64)*my_timeslot) + (NODE_DELAY*15));
      			}
    		}
  	}
	PROCESS_END();
}
