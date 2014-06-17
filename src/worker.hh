/* Worker thread to execute Vhayu Trade Analytics on demand.
 */

#ifndef WORKER_HH_
#define WORKER_HH_

#include <cstdint>
#include <memory>

/* Boost threading */
#include <boost/thread.hpp>

/* ZeroMQ messaging middleware. */
#include <zmq.h>

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>

#include "chromium/debug/leak_tracker.hh"
#include "googleurl/url_parse.h"
#include "chromium/string_piece.hh"

/* Maximum encoded size of an RSSL provider to client message. */
#define MAX_MSG_SIZE 4096

namespace vta
{
	class bar_t;
	class rollup_bar_t;
	class test_t;
}

namespace vhayu
{
	class permdata_t;
}

namespace hitsuji
{
	class provider_t;
	class MessageHeader;
	class Request;
	class Reply;

	class worker_t
	{
	public:
		explicit worker_t (std::shared_ptr<void>& zmq_context);
		virtual ~worker_t();

		bool Initialize (size_t id);
		void Reset();

/* Run core event loop. */
		void MainLoop();

		bool OnTask (const void* buffer, size_t length);

	private:
/* Per thread workspace. */
		bool AcquireFlexRecordCursor();

		bool SendReply (uintptr_t handle, int32_t token);

/* unique id per worker for trace. */
		std::string prefix_;

/* ZMQ context. */
		std::shared_ptr<void> zmq_context_;
		std::shared_ptr<void> request_sock_;
		std::shared_ptr<void> reply_sock_;
/* As worker state: */
/* Parsing state for requested items. */
		std::string url_;
		std::string underlying_symbol_;
/* Permission data */
		std::shared_ptr<vhayu::permdata_t> permdata_;
		std::string dacs_lock_;
/* FlexRecord cursor */
		FlexRecDefinitionManager* manager_;
		std::shared_ptr<FlexRecWorkAreaElement> work_area_;
		std::shared_ptr<FlexRecViewElement> view_element_;
/* ZMQ message */
		zmq_msg_t zmq_msg_;
/* Sbe message buffer */
		std::shared_ptr<MessageHeader> sbe_hdr_;
		std::shared_ptr<Request> sbe_request_;
		std::shared_ptr<Reply> sbe_reply_;
		char sbe_buf_[MAX_MSG_SIZE];
		size_t sbe_length_;
/* Rssl message buffer */
		char rssl_buf_[MAX_MSG_SIZE];
		size_t rssl_length_;
/* Analytics */
		std::shared_ptr<vta::bar_t> vta_bar_;
		std::shared_ptr<vta::rollup_bar_t> vta_rollup_bar_;
		std::shared_ptr<vta::test_t> vta_test_;

		chromium::debug::LeakTracker<worker_t> leak_tracker_;
	};

} /* namespace hitsuji */

#endif /* WORKER_HH_ */

/* eof */