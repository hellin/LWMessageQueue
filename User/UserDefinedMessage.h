#pragma once

namespace LWMessageQueue {

namespace User {

// Declare message data structs.

//struct <Message_name>Data {
//	<type> <variable_name>;
//	...
//};

struct Message1Data {
	uint32_t value;
};

// End of message data structs.

// Add the message to the type enum.
enum class MessageType {
	//<Mesage_name>
	Message1
};

// Add the message data struct to the data union.
union MessageData {
	//<Message_name>Data <Message_name>;
	Message1Data message1;
};

} // namespace User

} // namespace LWMessageQueue
