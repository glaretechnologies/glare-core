/*=====================================================================
EmscriptenWebSocket.cpp
-----------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "EmscriptenWebSocket.h"


#include "../maths/mathstypes.h"
#include "../utils/BitUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/PlatformUtils.h"
#include "../utils/Lock.h"
#include "MySocket.h"


EmscriptenWebSocket::EmscriptenWebSocket()
{}


static EM_BOOL onOpenCallback(int event_type, const EmscriptenWebSocketOpenEvent* websocket_event, void *user_data)
{
	conPrint("onOpenCallback");

	return EM_TRUE; // TODO: This right?
}


static EM_BOOL onErrorCallback(int event_type, const EmscriptenWebSocketErrorEvent* websocket_event, void *user_data)
{
	conPrint("onErrorCallback");

	return EM_TRUE; // TODO: This right?
}


static EM_BOOL onCloseCallback(int event_type, const EmscriptenWebSocketCloseEvent* websocket_event, void *user_data)
{
	conPrint("onCloseCallback");

	return EM_TRUE; // TODO: This right?
}


static EM_BOOL onMessageCallback(int event_type, const EmscriptenWebSocketMessageEvent* websocket_event, void *user_data)
{
	// conPrint("onMessageCallback, enqueuing " + toString(websocket_event->numBytes) + " bytes into read buffer");

	assert(!websocket_event->isText);

	EmscriptenWebSocket* websock = (EmscriptenWebSocket*)user_data;
	
	// Buffer data
	{
		Lock lock(websock->mutex);
		websock->read_buffer.pushBackNItems(websocket_event->data, websocket_event->numBytes);
	}
	websock->new_data_condition.notifyAll(); // Notify all suspended threads that there are new items in the queue. 

	return EM_TRUE; // TODO: This right?
}


void EmscriptenWebSocket::connect(const std::string& protocol, const std::string& hostname, int port)
{
	const std::string URL = protocol + "://" + hostname + ":" + toString(port);

	conPrint("EmscriptenWebSocket::connect(), URL: " + URL);

	EmscriptenWebSocketCreateAttributes attr;
	emscripten_websocket_init_create_attributes(&attr);
	attr.url = URL.c_str();
	attr.protocols = "binary";
	attr.createOnMainThread = EM_FALSE; // TEMP

	socket = emscripten_websocket_new(&attr);
	if(socket <= 0)
	{
		conPrint("EmscriptenWebSocket::connect() failed to create websocket ");
		throw glare::Exception("Failed to create websocket: " + toString(socket));
	}

	emscripten_websocket_set_onopen_callback (socket, /*userdata=*/this, onOpenCallback);
	emscripten_websocket_set_onerror_callback(socket, /*userdata=*/this, onErrorCallback);
	emscripten_websocket_set_onclose_callback(socket, /*userdata=*/this, onCloseCallback);
	emscripten_websocket_set_onmessage_callback(socket, /*userdata=*/this, onMessageCallback);


	// Block until we are in connected state
	while(1)
	{
		// Get state
		unsigned short ready_state = 0;
		emscripten_websocket_get_ready_state(socket, &ready_state);
		if(ready_state == 0) // Connecting state (see https://developer.mozilla.org/en-US/docs/Web/API/WebSocket/readyState)
		{
			PlatformUtils::Sleep(10);
			//conPrint("In connecting state, waiting...");
		}
		else if(ready_state == 1) // Open state
		{
			conPrint("In open state");
			break;
		}
		else if(ready_state == 2) // closing state
			throw glare::Exception("Socket closing while connecting");
		else if(ready_state == 3) // closed state
			throw glare::Exception("Socket closed while connecting");
		else
			throw glare::Exception("Socket has invalid ready state while connecting");
	}

	conPrint("EmscriptenWebSocket::connect() done.");
}


size_t EmscriptenWebSocket::readSomeBytes(void* buffer, size_t max_num_bytes)
{
	assert(0);
	return 0;
}


void EmscriptenWebSocket::ungracefulShutdown()
{
	closing = 1;

	emscripten_websocket_delete(socket);

	new_data_condition.notify(); // Wake up any threads suspended in readTo()
}


void EmscriptenWebSocket::waitForGracefulDisconnect()
{

}


void EmscriptenWebSocket::startGracefulShutdown()
{
	closing = 1;

	emscripten_websocket_close(socket,
		1000, // close code: 1001 = "endpoint going away", such as "browser having navigated away from a page". (https://datatracker.ietf.org/doc/html/rfc6455#section-7.4)
		// However get an error using code 1001 so just use code 1000.
		"" // close reason
	);

	new_data_condition.notify(); // Wake up any threads suspended in readTo()
}


void EmscriptenWebSocket::readTo(void* buffer, size_t readlen)
{
	Lock lock(mutex); // Lock read-buffer

	// Suspend thread until read-buffer is large enough.  Don't suspend if we are trying to close the socket connection though.
	while((read_buffer.size() < readlen) && (closing == 0))
		new_data_condition.wait(mutex); 

	if(closing)
		throw glare::Exception("Socket is closing");

	read_buffer.popFrontNItems(/*dest=*/(uint8*)buffer, readlen);
}


void EmscriptenWebSocket::writeInt32(int32 x)
{
	emscripten_websocket_send_binary(socket, &x, sizeof(int32));
}


void EmscriptenWebSocket::writeUInt32(uint32 x)
{
	emscripten_websocket_send_binary(socket, &x, sizeof(uint32));
}


int EmscriptenWebSocket::readInt32()
{
	uint32 i;
	readTo(&i, sizeof(uint32));
	return bitCast<int32>(i);
}


uint32 EmscriptenWebSocket::readUInt32()
{
	uint32 x;
	readTo(&x, sizeof(uint32));
	return x;
}


void EmscriptenWebSocket::readData(void* buf, size_t num_bytes)
{
	readTo(buf, num_bytes);
}


bool EmscriptenWebSocket::endOfStream()
{
	return false;
}


void EmscriptenWebSocket::writeData(const void* data, size_t num_bytes)
{
	emscripten_websocket_send_binary(socket, (void*)data, num_bytes);
}


#if BUILD_TESTS


#include "../utils/ConPrint.h"
#include "../utils/TestUtils.h"


void EmscriptenWebSocket::test()
{
	
}


#endif // BUILD_TESTS
