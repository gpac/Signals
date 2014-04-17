#pragma once

#include <../modules/modules.hpp>

namespace MM {

/**
 * A module, containing a delegate module.
 * This is the same principle as the current MM:Module preprocessor, but the relation between the input pin
 *  and the delegate is async but guarantees the packet order.
 * Therefore you should connect to this module synchronously.
 */
class Reorder : public Modules::Module {
public:
	Reorder(Modules::Module *module) : delegate(module) {
		signals.push_back(uptr(pinFactory->createPin())); //TODO: this super module should copy the structure from the delegate
		Connect(synchronizerSignal, [](std::shared_ptr<Data> sample) {
			return sample;
		});
		Connect(internalSignal, this, &Reorder::processInOrder);
		Connect(delegate->getPin(0)->getSignal(), this, &Reorder::reemit); //delegate output to this output ; faster is delegate output signal is sync
	}
	void waitForCompletion() {
		delegate->waitForCompletion();
		Modules::Module::waitForCompletion();
	}
	bool handles(const std::string &url) {
		return delegate->handles(url);
	}
	bool process(std::shared_ptr<Data> data) {
		synchronizerSignal.emit(data);
		internalSignal.emit();
		return true;
	}

private:
	std::shared_ptr<Data> waitForNextData() {
		return synchronizerSignal.results(true, true);
	}
	bool processInOrder() {
		auto data = waitForNextData(); //wait synchronously for the next
		return delegate->process(data);
	}
	bool reemit(std::shared_ptr<Data> data) { //output pin forwarding
		getPin(0)->getSignal().flushAvailableResults();
		getPin(0)->getSignal().emit(data);
		return true;
	}
	std::unique_ptr<Modules::Module> delegate;
	Signal<std::shared_ptr<Data>(std::shared_ptr<Data>), ResultLast<std::shared_ptr<Data>>> synchronizerSignal;
	Signal<bool(void), ResultQueueThreadSafe<bool>, CallerThread> internalSignal;
};

}
