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
	std::string api_path; // e.g. "/v1/messages"
	std::string api_key_credential_name; // Key used to look up API key credential, e.g. "openai_api_key"

	enum Provider
	{
		Provider_Anthropic,
		Provider_XAI,
		Provider_OpenAI,
		Provider_Google,
		Provider_Other
	};
	Provider provider;

	AIModel() : provider(Provider_Other) {}
};


class ToolFunctionSpec
{
public:
	std::string name;
	std::string description;

	// JSON schema for the function arguments, as raw JSON text, e.g.
	// {"type":"object", "properties":{"x":{"type":"number"}}, "required":["x"]}
	// If empty, the function is treated as taking no arguments.
	// Note that this is inserted into the request JSON verbatim, so it must be valid JSON.
	std::string input_schema_json;
};


class ToolFunctionsSpec
{
public:
	std::vector<ToolFunctionSpec> funcs;
};


class ToolFunctionCall : public ThreadSafeRefCounted
{
public:
	std::string call_id;
	std::string function_name;

	// The function arguments, as raw JSON object text, e.g. {"x":1.0,"name":"cube"}.
	// Empty if the model called the function with no arguments.
	// Callers should parse this with JSONParser; it comes from the LLM so is not trusted to match the schema.
	std::string args_json;

	std::string extra_content_json; // Used by Gemini to pass 'thought_signature' extra_content JSON.
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


// A thinking (reasoning) block emitted by the LLM.  Anthropic only.
// When thinking is enabled, thinking blocks must be sent back to the server verbatim as the first content blocks of the
// assistant message they came from, otherwise the server rejects assistant messages that contain tool calls.
// Note that the thinking text can be empty (e.g. when thinking display is 'omitted', which is the default for recent
// models); such blocks must still be sent back.
class ThinkingBlock
{
public:
	ThinkingBlock() : content_block_index(-1), redacted(false) {}

	int content_block_index; // Index of the content block in the streaming response, used to match up deltas with blocks.  Not sent to the server.
	bool redacted; // If true, this is a 'redacted_thinking' block, and 'data' is used instead of 'thinking' and 'signature'.
	std::string thinking; // Unescaped.  May be empty.
	std::string signature; // Opaque signature, must be passed back unmodified.
	std::string data; // Opaque data for redacted_thinking blocks, must be passed back unmodified.
};


struct LLMChatMessage
{
	enum Role
	{
		Role_User = 0,
		Role_Assistant = 1,
		Role_Tool = 2 // NOTE: Anthropic doesn't have a Role_Tool, it just uses Role_User.
	};
	Role role;
	std::string content; // Unescaped.

	std::vector<ThinkingBlock> thinking_blocks; // Thinking blocks the LLM emitted, in the order they were received.  For Role_Assistant, Anthropic only.

	std::vector<Reference<ToolFunctionCall>> tool_calls; // Tool calls the LLM made. For Role_Assistant

	std::vector<ToolCallResult> tool_call_results; // Results being returned to the LLM.  For Role_User (anthropic) and Role_Tool (OpenAI compatible).
};



class LLMClientHandlerInterface
{
public:
	virtual void responseDataReceived(const std::string& /*data*/) {};
	virtual void toolFunctionCallsReceived(const Reference<AIToolFunctionCalls>& /*function_calls*/) {};
	virtual void responseDone() {};
	virtual void modelIsThinking() {};
};



/*=====================================================================
LLMClient
---------
Handles the client side of communication with a LLM cloud server.
Synchronous.


TODO:
trimChatMessageHistory() (:435) pops from the front with no awareness of pairing. LLMThread::Settings::max_num_messages is 100 (ai/LLMThread.h:87), so
  this will fire in real sessions and can (a) drop an assistant tool_use while keeping the user tool_result that references it _ unexpected tool_result,
  and (b) leave the history starting with an assistant message, which Anthropic rejects. Trimming needs to pop whole user/assistant/tool-result groups,
  and stop at a Role_User boundary.
=====================================================================*/
class LLMClient : public HTTPClient::StreamingDataHandler, public ThreadSafeRefCounted
{
public:
	LLMClient(const AIModel& AI_model, const ToolFunctionsSpec& tool_functions, const std::string& base_prompt, const SimpleCredentials* credentials, LLMClientHandlerInterface* handler);
	virtual ~LLMClient();


	struct SendResult
	{
		enum SendResultType
		{
			SendResultType_NoSend,
			SendResultType_SendSucceeded,
			SendResultType_SendFailed
		};
		SendResultType type;
	};

	SendResult appendChatMessage(const std::string& message, bool should_send_to_server_immediately);

	SendResult appendToolCallResult(const ToolCallResult& result, bool should_send_to_server_immediately);


	
	// HTTPClient::StreamingDataHandler interface
	virtual void handleData(ArrayRef<uint8> chunk, const HTTPClient::ResponseInfo& response_info) override;

	static void test();
private:
	LLMClient::SendResult sendChatRequestToLLMServer();
	Reference<HTTPClient> createHTTPClient();
	void trimChatMessageHistory();

	LLMClientHandlerInterface* handler;

	const SimpleCredentials* credentials;

	int next_nonempty_line_start;
	int newline_search_pos;
	std::vector<char> http_response_data; // We will stream http response data into here, and search for newlines with next_nonempty_line_start and newline_search_pos

	AIModel cur_ai_model;

	std::deque<LLMChatMessage> chat_messages; // Chat message history
public:
	std::string base_prompt_json_escaped;
	std::string tools_json;

	size_t max_num_messages; // Maximum number of messages to keep in chat history.  If exceeded, the oldest messages are removed.
	size_t max_tokens;

	enum ReasoningEffort
	{
		ReasoningEffort_low,
		ReasoningEffort_med,
		ReasoningEffort_high,
		ReasoningEffort_xhigh, // extra-high
		ReasoningEffort_max, // extra-high
	};
	ReasoningEffort reasoning_effort; // low by default

	LLMChatMessage current_assistant_response; // Accumulated complete response from the LLM

private:
	Reference<HTTPClient> m_http_client; // For connecting to AI/LLM server
};
