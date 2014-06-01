/* Worker thread to execute Vhayu Trade Analytics on demand.
 */

#ifndef WORKER_HH_
#define WORKER_HH_

#include <cstdint>
#include <memory>

/* Boost threading */
#include <boost/thread.hpp>

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>

#include "googleurl/url_parse.h"
#include "chromium/string_piece.hh"

/* Maximum encoded size of an RSSL provider to client message. */
#define MAX_MSG_SIZE 4096

namespace vta
{
	class bar_t;
	class test_t;
}

namespace hitsuji
{
	class provider_t;
	class MessageHeader;
	class Request;

	class worker_t
	{
	public:
		explicit worker_t (std::shared_ptr<provider_t> provider);
		virtual ~worker_t();

		bool Start();
		void Stop();

		bool Initialize();
		void Reset();

		bool OnTask (const void* buffer, size_t length);

	private:
/* Run core event loop. */
		void MainLoop();
/* Per thread workspace. */
		bool AcquireFlexRecordCursor();

/* UPA provider */
		std::shared_ptr<provider_t> provider_;

/* unique id per worker for trace. */
		std::string prefix_;

/* As worker state: */
/* Parsing state for requested items. */
		std::string url_;
		std::string underlying_symbol_;
/* FlexRecord cursor */
		FlexRecDefinitionManager* manager_;
		std::shared_ptr<FlexRecWorkAreaElement> work_area_;
		std::shared_ptr<FlexRecViewElement> view_element_;
/* Sbe message buffer */
		std::shared_ptr<MessageHeader> sbe_hdr_;
		std::shared_ptr<Request> sbe_msg_;
		char sbe_buf_[MAX_MSG_SIZE];
		size_t sbe_length_;
/* Rssl message buffer */
		char rssl_buf_[MAX_MSG_SIZE];
		size_t rssl_length_;
/* Analytics*/
		std::shared_ptr<vta::bar_t> vta_bar_;
		std::shared_ptr<vta::test_t> vta_test_;
	};

} /* namespace hitsuji */

#endif /* WORKER_HH_ */

/* eof */