#include <hash.h>
#include "vm/sft.h"
#include "threads/synch.h"

static struct hash sft;
static struct lock sft_lock;



