
#include <functional>
#include <memory>

namespace ockl {

template <typename R>
class RAII {
private:
	R* resource;
	std::function<void (R*)> free;

public:
	RAII(R* resource, const std::function<void (R*)>& free)
	: resource(resource),
	  free(free)
	{
	}

	~RAII() {
		if (resource != nullptr) {
			free(resource);
		}
	}

	void release() {
		resource = nullptr;
	}
};

} // namespace
