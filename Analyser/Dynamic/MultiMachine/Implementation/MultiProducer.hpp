//
//  MultiProducer.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef MultiProducer_hpp
#define MultiProducer_hpp

#include "../../../../Concurrency/AsyncTaskQueue.hpp"
#include "../../../../Machines/MachineTypes.hpp"
#include "../../../../Machines/DynamicMachine.hpp"

#include "MultiSpeaker.hpp"

#include <memory>
#include <mutex>
#include <vector>

namespace Analyser {
namespace Dynamic {

template <typename MachineType> class MultiInterface {
	public:
		MultiInterface(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines, std::recursive_mutex &machines_mutex) :
			machines_(machines), machines_mutex_(machines_mutex), queues_(machines.size()) {}

	protected:
		/*!
			Performs a parallel for operation across all machines, performing the supplied
			function on each and returning only once all applications have completed.

			No guarantees are extended as to which thread operations will occur on.
		*/
		void perform_parallel(const std::function<void(MachineType *)> &);

		/*!
			Performs a serial for operation across all machines, performing the supplied
			function on each on the calling thread.
		*/
		void perform_serial(const std::function<void(MachineType *)> &);

	protected:
		const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines_;
		std::recursive_mutex &machines_mutex_;

	private:
		std::vector<Concurrency::TaskQueue<true>> queues_;
};

class MultiTimedMachine: public MultiInterface<MachineTypes::TimedMachine>, public MachineTypes::TimedMachine {
	public:
		using MultiInterface::MultiInterface;

		/*!
			Provides a mechanism by which a delegate can be informed each time a call to run_for has
			been received.
		*/
		struct Delegate {
			virtual void did_run_machines(MultiTimedMachine *) = 0;
		};
		/// Sets @c delegate as the receiver of delegate messages.
		void set_delegate(Delegate *delegate) {
			delegate_ = delegate;
		}

		void run_for(Time::Seconds duration) final;

	private:
		void run_for(const Cycles) final {}
		Delegate *delegate_ = nullptr;
};

class MultiScanProducer: public MultiInterface<MachineTypes::ScanProducer>, public MachineTypes::ScanProducer {
	public:
		using MultiInterface::MultiInterface;

		/*!
			Informs the MultiScanProducer that the order of machines has changed; it
			uses this as an opportunity to synthesis any CRTMachine::Machine::Delegate messages that
			are necessary to bridge the gap between one machine and the next.
		*/
		void did_change_machine_order();

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) final;
		Outputs::Display::ScanStatus get_scan_status() const final;

	private:
		Outputs::Display::ScanTarget *scan_target_ = nullptr;
};

class MultiAudioProducer: public MultiInterface<MachineTypes::AudioProducer>, public MachineTypes::AudioProducer {
	public:
		MultiAudioProducer(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines, std::recursive_mutex &machines_mutex);

		/*!
			Informs the MultiAudio that the order of machines has changed; it
			uses this as an opportunity to switch speaker delegates as appropriate.
		*/
		void did_change_machine_order();

		Outputs::Speaker::Speaker *get_speaker() final;

	private:
		MultiSpeaker *speaker_ = nullptr;
};

/*!
	Provides a class that multiplexes the CRT machine interface to multiple machines.

	Keeps a reference to the original vector of machines; will access it only after
	acquiring a supplied mutex. The owner should also call did_change_machine_order()
	if the order of machines changes.
*/

}
}


#endif /* MultiProducer_hpp */
