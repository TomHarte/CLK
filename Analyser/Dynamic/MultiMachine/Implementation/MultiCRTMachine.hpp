//
//  MultiCRTMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/01/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef MultiCRTMachine_hpp
#define MultiCRTMachine_hpp

#include "../../../../Concurrency/AsyncTaskQueue.hpp"
#include "../../../../Machines/CRTMachine.hpp"
#include "../../../../Machines/DynamicMachine.hpp"

#include <memory>
#include <mutex>
#include <vector>

namespace Analyser {
namespace Dynamic {

class MultiCRTMachine: public ::CRTMachine::Machine, public ::CRTMachine::Machine::Delegate {
	public:
		MultiCRTMachine(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines, std::mutex &machines_mutex);

		void setup_output(float aspect_ratio) override;
		void close_output() override;
		Outputs::CRT::CRT *get_crt() override;
		Outputs::Speaker::Speaker *get_speaker() override;
		void run_for(const Cycles cycles) override;
		double get_clock_rate() override;
		bool get_clock_is_unlimited() override;
		void set_delegate(::CRTMachine::Machine::Delegate *delegate) override;

		void machine_did_change_clock_rate(Machine *machine) override;
		void machine_did_change_clock_is_unlimited(Machine *machine) override;

		void did_change_machine_order();

		struct Delegate {
			virtual void multi_crt_did_run_machines() = 0;
		};
		void set_delegate(Delegate *delegate) {
			delegate_ = delegate;
		}

	private:
		const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines_;
		std::mutex &machines_mutex_;
		std::vector<Concurrency::AsyncTaskQueue> queues_;
		Delegate *delegate_ = nullptr;

		/*!
			Performs a parallel for operation across all machines, performing the supplied
			function on each and returning only once all applications have completed.

			No guarantees are extended as to which thread operations will occur on.
		*/
		void perform_parallel(const std::function<void(::CRTMachine::Machine *)> &);

		/*!
			Performs a serial for operation across all machines, performing the supplied
			function on each on the calling thread.
		*/
		void perform_serial(const std::function<void(::CRTMachine::Machine *)> &);
};

}
}


#endif /* MultiCRTMachine_hpp */
