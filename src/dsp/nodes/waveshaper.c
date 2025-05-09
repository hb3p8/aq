/* node_waveshaper.c  (dry / wet blend) ----------------------------- */
#include "../node.h"
#include <math.h>

static const char* shape_strings[] = { "tanh","hardclip","asin","off", NULL };
enum { TANH, HARDCLIP, ASIN, OFF };

typedef struct {
  Node node;
  int  shape;
  NodePort in, gain;   /* gain inlet is now “amount” 0…1 */
  NodePort out;
} WaveshaperNode;

/* ----- helpers ---------------------------------------------------- */
#define hardclip(x)   clampf((x), -1.f, 1.f)
#define asin_shape(x) asinf(hardclip(x))

/* fixed pre-gain that actually drives the non-linearity */
#define PREG  2.f                     /* ≃ +15.6 dB – tweak to taste   */

/* dry / wet processor */
#define process_loop(fn)                                             \
  for (int i = 0; i < NODE_BUFFER_SIZE; i++) {                       \
    float amt = clampf(n->gain.buf[i], 0.f, 1.f); /* 0…1 */          \
    float dry = n->in.buf[i];                                        \
    float wet = fn(dry * PREG) / PREG;   /* equalised output level */\
    n->out.buf[i] = dry + amt * (wet - dry);                         \
  }

/* ----- audio callback --------------------------------------------- */
static void process(Node* node)
{
  WaveshaperNode* n = (WaveshaperNode*)node;

  switch (n->shape) {
  case TANH: process_loop(tanhf);      break;
  case HARDCLIP: process_loop(hardclip);   break;
  case ASIN: process_loop(asin_shape); break;
  case OFF: memcpy(n->out.buf, n->in.buf, sizeof n->out.buf); break;
  }

  node_process(node);
}

/* ----- message handler -------------------------------------------- */
static int receive(Node* node, const char* msg, char* err)
{
  WaveshaperNode* n = (WaveshaperNode*)node;
  char buf[16];

  if (sscanf(msg, "shape %15s", buf)) {
    int idx = string_to_enum(shape_strings, buf);
    if (idx < 0) { sprintf(err, "bad shape '%s'", buf); return -1; }
    n->shape = idx;
    return 0;
  }
  sprintf(err, "bad command"); return -1;
}

/* ----- constructor ------------------------------------------------ */
Node* new_waveshaper_node(void)
{
  WaveshaperNode* node = calloc(1, sizeof * node);

  static const char* inlets[] = { "in", "gain", NULL };
  static const char* outlets[] = { "out", NULL };

  static NodeInfo info = {
    .name = "waveshaper",
    .inlets = inlets,
    .outlets = outlets,
  };

  static NodeVtable vt = {
    .process = process,
    .receive = receive,
    .free = node_free,
  };

  node_init(&node->node, &info, &vt, &node->in, &node->out);

  node_set(&node->node, "gain", 0.0f);  /* amount = 0 → fully dry */
  node->shape = TANH;

  return &node->node;
}