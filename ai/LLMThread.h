/*=====================================================================
LLMThread.h
-----------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "LLMClient.h"
#include "LLMThreadUser.h"
#include <MessageableThread.h>
#include <Platform.h>
#include <WeakReference.h>
#include <AtomicInt.h>
#include <string>
class Server;
class SimpleCredentials;
class EventFD;


// Append a user chat message to the chat history, and send to LLM server.
class SendAIChatPostContent : public ThreadMessage
{
public:
	SendAIChatPostContent() : should_send_to_server_immediately(true)  {}

	std::string message; // Unescaped.

	bool should_send_to_server_immediately;
};


// Append a tool call function result to the chat history, and send to LLM server.
class SendAIChatToolCallResult : public ThreadMessage
{
public:
	SendAIChatToolCallResult() : should_send_to_server_immediately(true)  {}

	ToolCallResult tool_call_result;

	bool should_send_to_server_immediately;
};


// The LLM has responded with a message.
class AIChatResponseDataMessage : public ThreadMessage
{
public:
	std::string message;
	WeakReference<LLMThreadUser> user;
};


// The LLM has responded with a tool function-call message.
class AIToolFunctionCallMessage : public ThreadMessage
{
public:
	Reference<AIToolFunctionCalls> calls;
	WeakReference<LLMThreadUser> user;
};


// The LLM has finished streaming a response.
class AIChatResponseDoneMessage : public ThreadMessage
{
public:
	WeakReference<LLMThreadUser> user;
};


/*=====================================================================
LLMThread
---------
Handles the client side of communication with a LLM cloud server.

Receives SendAIChatPostContent, SendAIChatToolCallResult messages on its thread queue.

Sends back AIChatResponseDataMessage, AIToolFunctionCallMessage, AIChatResponseDoneMessage messages 
on out_msg_queue.
=====================================================================*/
class LLMThread : public LLMClientHandlerInterface, public MessageableThread
{
public:
	class Settings
	{
	public:
		Settings() : max_num_messages(100) {}

		ToolFunctionsSpec tool_functions;
		std::string base_prompt;
		size_t max_num_messages; // Maximum number of messages to keep in chat history.  If exceeded, the oldest messages are removed.
	};

	LLMThread(const std::string& AI_model_id, const Settings& settings, const SimpleCredentials* credentials, ThreadSafeQueue<ThreadMessageRef>* out_msg_queue);
	virtual ~LLMThread();

	virtual void doRun() override;

	virtual void kill() override;


	// Internal: LLMClientHandlerInterface interface:
	virtual void responseDataReceived(const std::string& data) override;
	virtual void toolFunctionCallsReceived(const Reference<AIToolFunctionCalls>& function_calls) override;
	virtual void responseDone() override;

	// A reference to some object that is a user of this class.
	// Not directly used in this class, just passed along with evert response message queued on out_msg_queue.
	WeakReference<LLMThreadUser> user; // User objects may have a strong reference to this ob, so use a weak reference to avoid cycles.

	// Optional.  If non-null, this is notified after each response message is enqueued on out_msg_queue.
	// Used to wake a consumer that blocks on an event fd (e.g. a socket-reading thread) rather than on the queue itself.
	EventFD* out_msg_queue_event_fd;

private:
	std::string AI_model_id;
	Settings settings;
	const SimpleCredentials* credentials;
	ThreadSafeQueue<ThreadMessageRef>* out_msg_queue;

	glare::AtomicInt should_quit;
};
