#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#include "net/ipv6/multicast/uip-mcast6-engines.h"

/* multicast engine
*/
#define UIP_MCAST6_CONF_ENGINE UIP_MCAST6_ENGINE_SMRF

/* IPv6 settings
*/
#undef UIP_CONF_IPV6_RPL
#undef UIP_CONF_ND6_SEND_RA
#undef UIP_CONF_ROUTER
#define UIP_CONF_ND6_SEND_RA         0
#define UIP_CONF_ROUTER              0
#define UIP_MCAST6_ROUTE_CONF_ROUTES 0

#undef UIP_CONF_TCP
#define UIP_CONF_TCP 0

/* Code/RAM footprint savings
*/
#undef UIP_CONF_DS6_NBR_NBU
#undef UIP_CONF_DS6_ROUTE_NBU
#define UIP_CONF_DS6_NBR_NBU        10
#define UIP_CONF_DS6_ROUTE_NBU      10

/* RDC & MAC driver
*/
#undef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC nullrdc_driver
#undef NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE
#define NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE 16

#undef NETSTACK_CONF_MAC
#define NETSTACK_CONF_MAC nullmac_driver


/* multi channel adjustments
*/
#define TX_POWER 31		// radio tranmission power
#define CHANNELS 3		// amount of channels
#define CHANNEL_A 16
#define CHANNEL_B 19
#define CHANNEL_C 22
//#define CHANNEL_D 25		// add channels
#define START_SWITCH 21		// start changing channels after x rounds


/* COOJA simulation only!!
 * set to 1, to enable automatic node id configuration & debugging prints
*/
#define COOJA 0


/* experiment settings
*/
#define NODE_AMOUNT 10		// amount of nodes in grid
#define NODE_ID 2		// set node ID to flash device (not needed in COOJA)
#define NODE_DELAY 0		// spread transmission intervals
#define START_DELAY 60
#define ITERATIONS 6000
#define MCAST_SINK_UDP_PORT 3001


#endif
