/*=====================================================================
LLMClient.h
-----------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include <networking/HTTPClient.h>
#include <string>
#include <deque>
class SimpleCredentials;


struct AIModel
{
	std::string id_string; // A global id, e.g. "together.ai/meta-llama/Llama-3.3-70B-Instruct-Turbo"
	std::string api_id_string; // The ID passed to the API, e.g. "meta-llama/Llama-3.3-70B-Instruct-Turbo"
	std::string name; // Name shown to user, e.g. "Llama-3.3-70B-Instruct-Turbo [together.ai]"
	std::string description; // Description shown to user, e.g. "Meta's open source model, hosted by together.ai"

	std::string api_domain; // e.g. api.openai.com
	std::string api_key_credential_name; // e.g. "openai_api_key"
};


class ToolFunctionSpec
{
public:
	std::string name;
	std::string description;
	// TEMP: just assuming all functions take zero parameters for now.
};


class ToolFunctionsSpec
{
public:
	std::vector<ToolFunctionSpec> funcs;
};


class ToolFunctionArg : public ThreadSafeRefCounted
{
public:
};


class ToolFunctionCall : public ThreadSafeRefCounted
{
public:
	std::string call_id;
	std::string function_name;
	std::vector<Reference<ToolFunctionArg>> args;
};


// The LLM has responded with a tool function-call message.
class AIToolFunctionCalls : public ThreadSafeRefCounted
{
public:
	std::vector<Reference<ToolFunctionCall>> calls;
};


class ToolCallResult
{
public:
	std::string tool_call_id; // For Role_Tool
	std::string tool_call_name; // For Role_Tool
	std::string content; // Unescaped.
};


struct LLMChatMessage
{
	enum Role
	{
		Role_User = 0,
		Role_Assistant = 1,
		Role_Tool = 2
	};
	Role role;
	std::string tool_call_id; // For Role_Tool
	std::string tool_call_name; // For Role_Tool
	std::string content; // Unescaped.

	std::vector<Reference<ToolFunctionCall>> tool_calls; // For Role_Assistant
};



class LLMClientHandlerInterface
{
public:
	virtual void responseDataReceived(const std::string& /*data*/) {};
	virtual void toolFunctionCallsReceived(const Reference<AIToolFunctionCalls>& /*function_calls*/) {};
	virtual void responseDone() {};
};



/*=====================================================================
LLMClient
---------
Handles the client side of communication with a LLM cloud server.
Synchronous.
=====================================================================*/
class LLMClient : public HTTPClient::StreamingDataHandler, public ThreadSafeRefCounted
{
public:
	LLMClient(const AIModel& AI_model, const ToolFunctionsSpec& tool_functions, const std::string& base_prompt, const SimpleCredentials* credentials, LLMClientHandlerInterface* handler);
	virtual ~LLMClient();


	void appendChatMessage(const std::string& message, bool should_send_to_server_immediately);

	void appendToolCallResult(const ToolCallResult& result, bool should_send_to_server_immediately);


	
	// HTTPClient::StreamingDataHandler interface
	virtual void handleData(ArrayRef<uint8> chunk, const HTTPClient::ResponseInfo& response_info) override;

private:
	void sendChatRequestToLLMServer();
	Reference<HTTPClient> createHTTPClient();
	void trimChatMessageHistory();

	LLMClientHandlerInterface* handler;

	const SimpleCredentials* credentials;

	int next_nonempty_line_start;
	int newline_search_pos;
	std::vector<char> http_response_data; // We will stream http response data into here, and search for newlines with next_nonempty_line_start and newline_search_pos

	AIModel cur_ai_model;
	bool cur_ai_model_is_anthropic;

	std::deque<LLMChatMessage> chat_messages; // Chat message history
public:
	std::string base_prompt_json_escaped;
	std::string tools_json;

	size_t max_num_messages;
	size_t max_tokens;

	LLMChatMessage current_assistant_response; // Accumulated complete response from the LLM

private:
	Reference<HTTPClient> m_http_client; // For connecting to AI/LLM server
};
