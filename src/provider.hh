/* UPA interactive provider.
 */

#ifndef PROVIDER_HH_
#define PROVIDER_HH_

#include <winsock2.h>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>

/* Boost Atomics */
#include <boost/atomic.hpp>

/* Boost Posix Time */
#include <boost/date_time/posix_time/posix_time.hpp>

/* Boost threading. */
#include <boost/thread.hpp>

/* UPA 7.2 */
#include <upa/upa.h>

#include "chromium/debug/leak_tracker.hh"
#include "chromium/string_piece.hh"
#include "upa.hh"
#include "client.hh"
#include "config.hh"
#include "deleter.hh"

namespace hitsuji
{
/* Performance Counters */
	enum {
		PROVIDER_PC_BYTES_RECEIVED,
		PROVIDER_PC_UNCOMPRESSED_BYTES_RECEIVED,
		PROVIDER_PC_MSGS_SENT,
		PROVIDER_PC_RSSL_MSGS_ENQUEUED,
		PROVIDER_PC_RSSL_MSGS_SENT,
		PROVIDER_PC_RSSL_MSGS_RECEIVED,
		PROVIDER_PC_RSSL_MSGS_DECODED,
		PROVIDER_PC_RSSL_MSGS_MALFORMED,
		PROVIDER_PC_RSSL_MSGS_VALIDATED,
		PROVIDER_PC_CONNECTION_RECEIVED,
		PROVIDER_PC_CONNECTION_REJECTED,
		PROVIDER_PC_CONNECTION_ACCEPTED,
		PROVIDER_PC_CONNECTION_EXCEPTION,
		PROVIDER_PC_RWF_VERSION_UNSUPPORTED,
		PROVIDER_PC_RSSL_PING_SENT,
		PROVIDER_PC_RSSL_PONG_RECEIVED,
		PROVIDER_PC_RSSL_PONG_TIMEOUT,
		PROVIDER_PC_RSSL_PROTOCOL_DOWNGRADE,
		PROVIDER_PC_RSSL_FLUSH,
		PROVIDER_PC_OMM_ACTIVE_CLIENT_SESSION_RECEIVED,
		PROVIDER_PC_OMM_ACTIVE_CLIENT_SESSION_EXCEPTION,
		PROVIDER_PC_CLIENT_SESSION_REJECTED,
		PROVIDER_PC_CLIENT_SESSION_ACCEPTED,
		PROVIDER_PC_RSSL_RECONNECT,
		PROVIDER_PC_RSSL_CONGESTION_DETECTED,
		PROVIDER_PC_RSSL_SLOW_READER,
		PROVIDER_PC_RSSL_PACKET_GAP_DETECTED,
		PROVIDER_PC_RSSL_READ_FAILURE,
		PROVIDER_PC_CLIENT_INIT_EXCEPTION,
		PROVIDER_PC_DIRECTORY_MAP_EXCEPTION,
		PROVIDER_PC_RSSL_PING_EXCEPTION,
		PROVIDER_PC_RSSL_PING_FLUSH_FAILED,
		PROVIDER_PC_RSSL_PING_NO_BUFFERS,
		PROVIDER_PC_RSSL_WRITE_EXCEPTION,
		PROVIDER_PC_RSSL_WRITE_FLUSH_FAILED,
		PROVIDER_PC_RSSL_WRITE_NO_BUFFERS,
/* marker */
		PROVIDER_PC_MAX
	};

	class client_t;

	class provider_t :
		public std::enable_shared_from_this<provider_t>
	{
	public:
/* Delegate handles specific behaviour of an worker reply. */
		class Delegate {
		public:
		    Delegate() {}

/* Return false on EAGAIN */
		    virtual bool OnRead() = 0;

		protected:
		    virtual ~Delegate() {}
		};

		explicit provider_t (const config_t& config, std::shared_ptr<upa_t> upa, Delegate* reply_delegate, SOCKET reply_sock, client_t::Delegate* request_delegate);
		~provider_t();

		bool Initialize();
/* Run the current MessageLoop. This blocks until Quit is called. */
		void Run();
/* Quit an earlier call to Run(). */
		void Quit();
		void Close();

		static bool WriteRawClose (uint16_t rwf_version, int32_t token, uint16_t service_id, uint8_t model_type, const chromium::StringPiece& item_name, bool use_attribinfo_in_updates, uint8_t stream_state, uint8_t status_code, const chromium::StringPiece& status_text, void* data, size_t* length);
		bool SendReply (RsslChannel*const handle, int32_t token, const void* buf, size_t length);

		uint16_t rwf_version() const {
			return min_rwf_version_.load();
		}
		const char* service_name() const {
			return config_.service_name.c_str();
		}
		uint16_t service_id() const {
			return service_id_;
		}

	private:
		bool DoWork();

		void OnConnection (RsslServer* rssl_sock);
		void RejectConnection (RsslServer* rssl_sock);
		void AcceptConnection (RsslServer* rssl_sock);

		void OnCanReadWithoutBlocking (RsslChannel* handle);
		void OnCanWriteWithoutBlocking (RsslChannel* handle);
		void Abort (RsslChannel* handle);
		void Close (RsslChannel* handle);

		void OnInitializingState (RsslChannel* handle);
		void OnActiveClientSession (RsslChannel* handle);
		void RejectClientSession (RsslChannel* handle, const char* address);
		bool AcceptClientSession (RsslChannel* handle, const char* address);

		void OnActiveState (RsslChannel* handle);
		void OnMsg (RsslChannel* handle, RsslBuffer* buf);

		bool GetDirectoryMap (RsslEncodeIterator*const it, const char* service_name, uint32_t filter_mask, unsigned map_action);
		bool GetServiceDirectory (RsslEncodeIterator*const it, const char* service_name, uint32_t filter_mask);
		bool GetServiceFilterList (RsslEncodeIterator*const it, uint32_t filter_mask);
		bool GetServiceInformation (RsslEncodeIterator*const it);
		bool GetServiceCapabilities (RsslEncodeIterator*const it);
		bool GetServiceDictionaries (RsslEncodeIterator*const it);
		bool GetServiceQoS (RsslEncodeIterator*const it);
		bool GetServiceState (RsslEncodeIterator*const it);
		bool GetServiceLoad (RsslEncodeIterator*const it);

		int Submit (RsslChannel* c, RsslBuffer* buf);
		int Ping (RsslChannel* c);

		void SetServiceId (uint16_t service_id) {
			service_id_.store (service_id);
		}

		static uint8_t rwf_major_version (uint16_t rwf_version) { return rwf_version / 256; }
		static uint8_t rwf_minor_version (uint16_t rwf_version) { return rwf_version % 256; }

		const config_t& config_;

/* Reply socket to propagate events */
		SOCKET reply_sock_;
		Delegate* reply_delegate_;
/* UPA context. */
		std::shared_ptr<upa_t> upa_;
/* Server socket for new connections */
		RsslServer* rssl_sock_;
/* This flag is set to false when Run should return. */
		boost::atomic_bool keep_running_;

		int in_nfds_, out_nfds_;
		fd_set in_rfds_, in_wfds_, in_efds_;
		fd_set out_rfds_, out_wfds_, out_efds_;
		struct timeval in_tv_, out_tv_;

/* RSSL connection directory */
		std::list<RsslChannel*const> connections_;
/* RSSL Client Session directory */
		std::unordered_map<RsslChannel*const, std::shared_ptr<client_t>> clients_;
		boost::shared_mutex clients_lock_;

		client_t::Delegate* request_delegate_;
		friend client_t;

/* Reuters Wire Format versions. */
		boost::atomic_uint16_t min_rwf_version_;

/* Directory mapped ServiceID */
		boost::atomic_uint16_t service_id_;
/* TREP-RT can reject new client requests whilst maintaining current connected sessions. */
		bool is_accepting_connections_;
		bool is_accepting_requests_;

/** Performance Counters **/
		boost::posix_time::ptime creation_time_, last_activity_;
		uint32_t cumulative_stats_[PROVIDER_PC_MAX];
		uint32_t snap_stats_[PROVIDER_PC_MAX];

		chromium::debug::LeakTracker<provider_t> leak_tracker_;
	};

} /* namespace hitsuji */

#endif /* PROVIDER_HH_ */

/* eof */