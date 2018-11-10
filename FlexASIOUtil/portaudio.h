#pragma once

#include <portaudio.h>

#include <string>

namespace flexasio {

	class PortAudioLogger final {
	public:
		PortAudioLogger();
		~PortAudioLogger();
	};

	std::string GetHostApiTypeIdString(PaHostApiTypeId hostApiTypeId);

	struct HostApi {
		explicit HostApi(PaHostApiIndex index) : index(index), info(GetInfo(index)) {}

		const PaHostApiIndex index;
		const PaHostApiInfo& info;

		friend std::ostream& operator<<(std::ostream&, const HostApi&);

	private:
		static const PaHostApiInfo& GetInfo(PaHostApiIndex index);
	};

}