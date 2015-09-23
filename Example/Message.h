#include <stdint.h>

// An example on how to declare messages of different types that can all be sent to 
// the same message queue. Message structs have to be trivial POD types.

// Declare message data structs.
struct Message1 {
	uint32_t value;
	uint32_t anotherValue;
};

struct Message2 {
	uint32_t value;
	uint32_t anotherValue;
	uint8_t moreValues[2];
};

// Add the message data structs to a message union that is passed as a template parameter
// to LWMessageQueue.
union MessageUnion {
	Message1 message1;
	Message2 message2;
};
