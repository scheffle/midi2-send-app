// This file is subject to the license terms in the LICENSE file found in the top-level directory of
// this repository

#include "vstgui/standalone/include/helpers/appdelegate.h"
#include "vstgui/standalone/include/helpers/preferences.h"
#include "vstgui/standalone/include/helpers/uidesc/customization.h"
#include "vstgui/standalone/include/helpers/uidesc/modelbinding.h"
#include "vstgui/standalone/include/helpers/value.h"
#include "vstgui/standalone/include/helpers/windowcontroller.h"
#include "vstgui/standalone/include/helpers/windowlistener.h"
#include "vstgui/standalone/include/iapplication.h"
#include "vstgui/standalone/include/iuidescwindow.h"

#include "midi/midi2_channel_voice_message.h"
#include "midi/universal_packet.h"

#if __APPLE__
#include <CoreMIDI/CoreMIDI.h>
#include <mach/mach_time.h>
#endif

#include <sstream>
#include <iostream>
#include <iomanip>

using namespace VSTGUI::Standalone;

//------------------------------------------------------------------------
struct IMIDIClient
{
	virtual void send (const midi::universal_packet& packet) = 0;
};
using MIDIClientPtr = std::shared_ptr<IMIDIClient>;

#if __APPLE__
//------------------------------------------------------------------------
struct CoreMIDIClient : IMIDIClient
{
	CoreMIDIClient ()
	{
		auto status = MIDIClientCreateWithBlock (CFSTR ("MIDI2-Send"), &client,
												 ^(const MIDINotification* message) {
												 });
		assert (status == noErr);
		status =
			MIDISourceCreateWithProtocol (client, CFSTR ("MIDI2-Send"), kMIDIProtocol_2_0, &source);
		assert (status == noErr);
		Preferences prefs ("CoreMIDI");
		SInt32 sourceUniqueId;
		if (auto value = prefs.getNumber<SInt32> ("SourceUniqueID"))
		{
			sourceUniqueId = *value;
			MIDIObjectSetIntegerProperty (source, kMIDIPropertyUniqueID, sourceUniqueId);
		}
		else
		{
			MIDIObjectGetIntegerProperty (source, kMIDIPropertyUniqueID, &sourceUniqueId);
			prefs.setNumber ("SourceUniqueID", sourceUniqueId);
		}
	}

	~CoreMIDIClient () noexcept
	{
		auto& prefs = IApplication::instance ().getPreferences ();

		if (source)
			MIDIEndpointDispose (source);
		if (client)
			MIDIClientDispose (client);
	}

	void send (const midi::universal_packet& packet) override
	{
		MIDIEventList eventList {};
		auto p = MIDIEventListInit (&eventList, kMIDIProtocol_2_0);
		MIDIEventListAdd (&eventList, sizeof (MIDIEventList), p, mach_absolute_time (),
						  packet.size (), packet.data);
		auto status = MIDIReceivedEventList (source, &eventList);
		assert (status == noErr);
	}

	MIDIClientRef client {};
	MIDIEndpointRef source {};
};
#endif

//------------------------------------------------------------------------
struct HexRangeValueConverter : IValueConverter
{
	HexRangeValueConverter (const ValueConverterPtr& baseConverter) : baseConverter (baseConverter)
	{
	}
	VSTGUI::UTF8String valueAsString (IValue::Type value) const override
	{
		auto intValue = static_cast<uint64_t> (normalizedToPlain (value));
		std::stringstream s;
		try
		{
			s << "0x" << std::uppercase << std::hex << intValue;
		}
		catch (...)
		{
		}
		return {s.str ()};
	}
	IValue::Type stringAsValue (const VSTGUI::UTF8String& string) const override
	{
		uint32_t value;
		try
		{
			std::stringstream s (string.getString ());
			s << std::hex;
			s >> value;
		}
		catch (...)
		{
		}
		return plainToNormalized (value);
	}
	IValue::Type plainToNormalized (IValue::Type plain) const override
	{
		return baseConverter->plainToNormalized (plain);
	}
	IValue::Type normalizedToPlain (IValue::Type normalized) const override
	{
		return baseConverter->normalizedToPlain (normalized);
	}

	ValueConverterPtr baseConverter;
};

//------------------------------------------------------------------------
struct WindowController : WindowControllerAdapter,
						  UIDesc::CustomizationAdapter
{
	WindowController (MIDIClientPtr midiClient) : midiClient (midiClient)
	{
		model->addValue (Value::make ("Send"), UIDesc::ValueCalls::onEndEdit ([this] (auto& value) {
							 if (value.getValue () > 0.5)
							 {
								 doSendCommand ();
								 value.performEdit (0.);
							 }
						 }));
		model->addValue (Value::makeStringListValue ("MessageType", {"Note On", "Note Off"}));
		model->addValue (Value::makeStepValue ("Group", 16));
		model->addValue (Value::makeStepValue ("Channel", 16));
		model->addValue (Value::makeStepValue ("Pitch", 127));
		model->addValue (Value::make ("Velocity", 1., Value::makeRangeConverter (0., 100., 0)));
		model->addValue (Value::make (
			"AttributeType", 0.,
			std::make_shared<HexRangeValueConverter> (Value::makeRangeConverter (0., 127., 0))));
		model->addValue (Value::make (
			"AttributeValue", 0.,
			std::make_shared<HexRangeValueConverter> (Value::makeRangeConverter (0., 65535., 0))));
	}

	UIDesc::ModelBindingPtr getModel () const { return model; }

private:
	void doSendCommand ()
	{
		auto group = getGroupValue ();
		auto channel = getChannelValue ();
		auto pitch = getPitchValue ();
		auto velocity = getVelocityValue ();
		auto attribute = getAttributeTypeValue ();
		auto attribute_data = getAttributeValueValue ();
		midi::universal_packet message;
		auto messageType = getMessageTypeValue ();
		if (messageType == 0)
			message = midi::make_midi2_note_on_message (group, channel, pitch, velocity, attribute,
														attribute_data);
		else
		{
			// TODO: missing midi::make_midi2_note_off_message with attribute support
			message = midi::make_midi2_note_off_message (group, channel, pitch, velocity);
		}
		midiClient->send (message);
	}

	template<typename T>
	T getValue (VSTGUI::UTF8StringView valueName) const
	{
		if (auto value = model->getValue (valueName))
			return static_cast<T> (Value::currentPlainValue (*value));
		return {};
	}

	double getValue (VSTGUI::UTF8StringView valueName) const
	{
		if (auto value = model->getValue (valueName))
			return value->getValue ();
		return {};
	}

	uint8_t getAttributeTypeValue () const { return getValue<uint8_t> ("AttributeType"); }
	uint16_t getAttributeValueValue () const { return getValue<uint16_t> ("AttributeValue"); }
	midi::velocity getVelocityValue () const { return midi::velocity (getValue ("Velocity")); }
	uint8_t getPitchValue () const { return getValue<uint8_t> ("Pitch"); }
	uint8_t getGroupValue () const { return getValue<uint8_t> ("Group"); }
	uint8_t getChannelValue () const { return getValue<uint8_t> ("Channel"); }
	uint8_t getMessageTypeValue () const { return getValue<uint8_t> ("MessageType"); }

	MIDIClientPtr midiClient;
	UIDesc::ModelBindingCallbacksPtr model {UIDesc::ModelBindingCallbacks::make ()};
};

//------------------------------------------------------------------------
class App : public Application::DelegateAdapter,
			public WindowListenerAdapter
{
public:
	MIDIClientPtr midiClient;

	App () : Application::DelegateAdapter ({"MIDI2-Send", "1.0.0", VSTGUI_STANDALONE_APP_URI}) {}

	void finishLaunching () override
	{
#if __APPLE__
		midiClient = std::make_shared<CoreMIDIClient> ();
#endif
		auto controller = std::make_shared<WindowController> (midiClient);

		UIDesc::Config config;
		config.uiDescFileName = "Window.uidesc";
		config.viewName = "Window";
		config.modelBinding = controller->getModel ();
		config.customization = controller;
		config.windowConfig.title = "MIDI2-Send";
		config.windowConfig.autoSaveFrameName = "MainWindow";
		config.windowConfig.style.border ().close ().size ().centered ();
		if (auto window = UIDesc::makeWindow (config))
		{
			window->show ();
			window->registerWindowListener (this);
		}
		else
		{
			IApplication::instance ().quit ();
		}
	}
	void onClosed (const IWindow& window) override { IApplication::instance ().quit (); }
};

static Application::Init gAppDelegate (std::make_unique<App> ());
