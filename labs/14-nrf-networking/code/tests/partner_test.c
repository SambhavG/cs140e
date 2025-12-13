// one way test of 4-byte ack'd packets.  right now if it gets duplicate
// packets or perm packet loss it will assert out, but collisions/dups do
// not mean the code is wrong.
//
// you'll notice the bandwidth is awful!   a good extension/project is to
// improve this and see how close you can get to hw limits.
#include "nrf-test.h"

// useful to mess around with these.
enum { ntrial = 1000, timeout_usec = 10000000 };

// static void msg_set(uint32_t *m, unsigned i) {
//     m[0] = i;
//     m[1] = m[0]*m[0];
//     m[2] = m[1]*m[1];
//     m[3] = m[2]*m[2];
// }
static int msg_chk(uint32_t *exp, uint32_t *got) {
  int ok_p = 1;
  for (int i = 0; i < 4; i++) {
    if (exp[i] != got[i]) {
      output("expected msg[%d] = %d, expected %d\n", i, exp[i], got[i]);
      ok_p = 0;
    }
  }
  return ok_p;
}

// send 4 byte packets from <server> to <client>.
//
// nice thing about loopback is that it's trivial to know what we are
// sending, whether it got through, and do flow-control to coordinate
// sender and receiver.
// static void
// one_way_ack(nrf_t *server, nrf_t *client, int verbose_p) {
//     unsigned client_addr = client->rxaddr;
//     unsigned ntimeout = 0, npackets = 0;

//     uint32_t sent[4], recv[4];

//     for(unsigned i = 0; i < ntrial; i++) {
//         if(verbose_p && i  && i % 100 == 0)
//             trace("sent %d ack'd packets\n", i);

//         msg_set(sent,i);
//         nrf_send_ack(server, client_addr, sent, sizeof sent);

//         // receive from client nic
//         uint32_t x;
// if(nrf_read_exact_timeout(client, recv, sizeof recv, timeout_usec) == 16) {
//     if(!msg_chk(sent, recv))
//         nrf_output("client: corrupt packet=%d\n", i);
//     npackets++;
// } else {
//     if(verbose_p)
//         nrf_output("receive failed for packet=%d, timeout\n", i);
//     ntimeout++;
// }
//     }
//     trace("trial: total successfully sent %d ack'd packets lost [%d]\n",
//         npackets, ntimeout);
//     assert((ntimeout + npackets) == ntrial);
// }

char *message =
    "I have of late- but wherefore I know not- lost all my mirth, forgone all "
    "custom of exercises; and indeed, it goes so heavily with my disposition "
    "that this goodly frame, the earth, seems to me a sterile promontory; this "
    "most excellent canopy, the air, look you, this brave o'erhanging "
    "firmament, this majestical roof fretted with golden fire- why, it "
    "appeareth no other thing to me than a foul and pestilent congregation of "
    "vapours. What a piece of work is a man! how noble in reason! how infinite "
    "in faculties! in form and moving how express and admirable! in action how "
    "like an angel! in apprehension how like a god! the beauty of the world, "
    "the paragon of animals! And yet to me what is this quintessence of dust? "
    "Man delights not me- no, nor woman neither, though by your smiling you "
    "seem to say so...";

static void server_send(nrf_t *server) {

  char sent;

  for (unsigned i = 0; i < 804; i++) {
    sent = message[i];
    nrf_send_ack(server, client_addr, &sent, sizeof sent);
  }
}

static void client_receive(nrf_t *client) {
  char recv;
  for (unsigned i = 0; i < 804; i++) {
    if (nrf_read_exact_timeout(client, &recv, sizeof recv, timeout_usec) == 1) {
      printk("%c", recv);
    }
  }
  printk("\n");
}

void notmain(void) {
  unsigned nbytes = 1;
  kmalloc_init(1);

  // 0 for server, 1 for client
  uint32_t mode = 1;

  if (mode == 0) {
    // server waits 2s for client to start listening
    delay_ms(2000);
    nrf_t *s = server_mk_ack(server_addr, nbytes);
    nrf_stat_start(s);

    server_send(s);
    nrf_stat_print(s, "server: done with one-way test");

  } else {
    // Client starts listening and prints whatever it hears
    nrf_t *c = client_mk_ack(client_addr, nbytes);
    nrf_stat_start(c);

    client_receive(c);
    nrf_stat_print(c, "client: done with one-way test");
  }
}
