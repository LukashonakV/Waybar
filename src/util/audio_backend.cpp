#include "util/audio_backend.hpp"

#include <fmt/core.h>
#include <pulse/def.h>
#include <pulse/error.h>
#include <pulse/introspect.h>
#include <pulse/subscribe.h>
#include <pulse/volume.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace waybar::util {

AudioBackend::AudioBackend(std::function<void()> on_updated_cb, private_constructor_tag tag)
    : mainloop_(nullptr),
      mainloop_api_(nullptr),
      context_(nullptr),
      volume_(0),
      muted_(false),
      source_volume_(0),
      source_muted_(false),
      on_updated_cb_(std::move(on_updated_cb)) {
  // Initialize pa_volume_ with safe defaults
  pa_cvolume_init(&pa_volume_);
  mainloop_ = pa_threaded_mainloop_new();
  if (mainloop_ == nullptr) {
    throw std::runtime_error("pa_mainloop_new() failed.");
  }
  pa_threaded_mainloop_lock(mainloop_);
  mainloop_api_ = pa_threaded_mainloop_get_api(mainloop_);
  connectContext();
  if (pa_threaded_mainloop_start(mainloop_) < 0) {
    throw std::runtime_error("pa_mainloop_run() failed.");
  }
  pa_threaded_mainloop_unlock(mainloop_);
}

AudioBackend::~AudioBackend() {
  if (context_ != nullptr) {
    pa_context_disconnect(context_);
  }

  if (mainloop_ != nullptr) {
    mainloop_api_->quit(mainloop_api_, 0);
    pa_threaded_mainloop_stop(mainloop_);
    pa_threaded_mainloop_free(mainloop_);
  }
}

std::shared_ptr<AudioBackend> AudioBackend::getInstance(std::function<void()> on_updated_cb) {
  private_constructor_tag tag;
  return std::make_shared<AudioBackend>(on_updated_cb, tag);
}

void AudioBackend::connectContext() {
  context_ = pa_context_new(mainloop_api_, "waybar");
  if (context_ == nullptr) {
    throw std::runtime_error("pa_context_new() failed.");
  }
  pa_context_set_state_callback(context_, contextStateCb, this);
  if (pa_context_connect(context_, nullptr, PA_CONTEXT_NOFAIL, nullptr) < 0) {
    auto err =
        fmt::format("pa_context_connect() failed: {}", pa_strerror(pa_context_errno(context_)));
    throw std::runtime_error(err);
  }
}

void AudioBackend::contextStateCb(pa_context *c, void *data) {
  auto *backend = static_cast<AudioBackend *>(data);
  switch (pa_context_get_state(c)) {
    case PA_CONTEXT_TERMINATED:
      backend->mainloop_api_->quit(backend->mainloop_api_, 0);
      break;
    case PA_CONTEXT_READY:
      pa_context_get_server_info(c, serverInfoCb, data);
      pa_context_set_subscribe_callback(c, subscribeCb, data);
      pa_context_subscribe(c,
                           static_cast<enum pa_subscription_mask>(
                               static_cast<int>(PA_SUBSCRIPTION_MASK_SERVER) |
                               static_cast<int>(PA_SUBSCRIPTION_MASK_SINK) |
                               static_cast<int>(PA_SUBSCRIPTION_MASK_SINK_INPUT) |
                               static_cast<int>(PA_SUBSCRIPTION_MASK_SOURCE) |
                               static_cast<int>(PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT)),
                           nullptr, nullptr);
      break;
    case PA_CONTEXT_FAILED:
      // When pulseaudio server restarts, the connection is "failed". Try to reconnect.
      // pa_threaded_mainloop_lock is already acquired in callback threads.
      // So there is no need to lock it again.
      if (backend->context_ != nullptr) {
        pa_context_disconnect(backend->context_);
      }
      backend->connectContext();
      break;
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
    default:
      break;
  }
}

/*
 * Called when an event we subscribed to occurs.
 */
void AudioBackend::subscribeCb(pa_context *context, pa_subscription_event_type_t type, uint32_t idx,
                               void *data) {
  unsigned facility = type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;
  unsigned operation = type & PA_SUBSCRIPTION_EVENT_TYPE_MASK;
  if (operation != PA_SUBSCRIPTION_EVENT_CHANGE) {
    return;
  }
  if (facility == PA_SUBSCRIPTION_EVENT_SERVER) {
    pa_context_get_server_info(context, serverInfoCb, data);
  } else if (facility == PA_SUBSCRIPTION_EVENT_SINK) {
    pa_context_get_sink_info_by_index(context, idx, sinkInfoCb, data);
  } else if (facility == PA_SUBSCRIPTION_EVENT_SINK_INPUT) {
    pa_context_get_sink_info_list(context, sinkInfoCb, data);
  } else if (facility == PA_SUBSCRIPTION_EVENT_SOURCE) {
    pa_context_get_source_info_by_index(context, idx, sourceInfoCb, data);
  } else if (facility == PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT) {
    pa_context_get_source_info_list(context, sourceInfoCb, data);
  }
}

/*
 * Called in response to a volume change request
 */
void AudioBackend::volumeModifyCb(pa_context *c, int success, void *data) {
  auto *backend = static_cast<AudioBackend *>(data);
  if (success != 0) {
    if ((backend->context_ != nullptr) &&
        pa_context_get_state(backend->context_) == PA_CONTEXT_READY) {
      pa_context_get_sink_info_by_index(backend->context_, backend->sink_idx_, sinkInfoCb, data);
    }
  } else {
    spdlog::debug("Volume modification failed");
  }
}

/*
 * Called when the requested sink information is ready.
 */
void AudioBackend::sinkInfoCb(pa_context * /*context*/, const pa_sink_info *i, int /*eol*/,
                              void *data) {
  if (i == nullptr) return;

  auto running = i->state == PA_SINK_RUNNING;
  auto idle = i->state == PA_SINK_IDLE;
  spdlog::trace("Sink name {} Running:[{}] Idle:[{}]", i->name, running, idle);

  auto *backend = static_cast<AudioBackend *>(data);

  if (!backend->ignored_sinks_.empty()) {
    for (const auto &ignored_sink : backend->ignored_sinks_) {
      if (ignored_sink == i->description) {
        if (i->name == backend->current_sink_name_) {
          // If the current sink happens to be ignored it is never considered running
          // so it will be replaced with another sink.
          backend->current_sink_running_ = false;
        }

        return;
      }
    }
  }

  backend->default_sink_running_ = backend->default_sink_name == i->name &&
                                   (i->state == PA_SINK_RUNNING || i->state == PA_SINK_IDLE);

  if (i->name != backend->default_sink_name && !backend->default_sink_running_) {
    return;
  }

  if (backend->current_sink_name_ == i->name) {
    backend->current_sink_running_ = (i->state == PA_SINK_RUNNING || i->state == PA_SINK_IDLE);
  }

  if (!backend->current_sink_running_ &&
      (i->state == PA_SINK_RUNNING || i->state == PA_SINK_IDLE)) {
    backend->current_sink_name_ = i->name;
    backend->current_sink_running_ = true;
  }

  if (backend->current_sink_name_ == i->name) {
    // Safely copy the volume structure
    if (pa_cvolume_valid(&i->volume) != 0) {
      backend->pa_volume_ = i->volume;
      float volume =
          static_cast<float>(pa_cvolume_avg(&(backend->pa_volume_))) / float{PA_VOLUME_NORM};
      backend->sink_idx_ = i->index;
      backend->volume_ = std::round(volume * 100.0F);
    } else {
      spdlog::error("Invalid volume structure received from PulseAudio");
      // Initialize with safe defaults
      pa_cvolume_init(&backend->pa_volume_);
      backend->volume_ = 0;
    }

    backend->muted_ = i->mute != 0;
    backend->desc_ = i->description;
    backend->monitor_ = i->monitor_source_name;
    backend->port_name_ = i->active_port != nullptr ? i->active_port->name : "Unknown";
    if (const auto *ff = pa_proplist_gets(i->proplist, PA_PROP_DEVICE_FORM_FACTOR)) {
      backend->form_factor_ = ff;
    } else {
      backend->form_factor_ = "";
    }
    backend->on_updated_cb_();
  }
}

/*
 * Called when the requested source information is ready.
 */
void AudioBackend::sourceInfoCb(pa_context * /*context*/, const pa_source_info *i, int /*eol*/,
                                void *data) {
  auto *backend = static_cast<AudioBackend *>(data);
  if (i != nullptr && backend->default_source_name_ == i->name) {
    auto source_volume = static_cast<float>(pa_cvolume_avg(&(i->volume))) / float{PA_VOLUME_NORM};
    backend->source_volume_ = std::round(source_volume * 100.0F);
    backend->source_idx_ = i->index;
    backend->source_muted_ = i->mute != 0;
    backend->source_desc_ = i->description;
    backend->source_port_name_ = i->active_port != nullptr ? i->active_port->name : "Unknown";
    backend->on_updated_cb_();
  }
}

/*
 * Called when the requested information on the server is ready. This is
 * used to find the default PulseAudio sink.
 */
void AudioBackend::serverInfoCb(pa_context *context, const pa_server_info *i, void *data) {
  auto *backend = static_cast<AudioBackend *>(data);
  backend->current_sink_name_ = i->default_sink_name;
  backend->default_sink_name = i->default_sink_name;
  backend->default_source_name_ = i->default_source_name;

  pa_context_get_sink_info_list(context, sinkInfoCb, data);
  pa_context_get_source_info_list(context, sourceInfoCb, data);
}

void AudioBackend::changeVolume(uint16_t volume, uint16_t min_volume, uint16_t max_volume) {
  // Early return if context is not ready
  if ((context_ == nullptr) || pa_context_get_state(context_) != PA_CONTEXT_READY) {
    spdlog::error("PulseAudio context not ready");
    return;
  }

  // Prepare volume structure
  pa_cvolume pa_volume;

  pa_cvolume_init(&pa_volume);

  // Use existing volume structure if valid, otherwise create a safe default
  if ((pa_cvolume_valid(&pa_volume_) != 0) && (pa_channels_valid(pa_volume_.channels) != 0)) {
    pa_volume = pa_volume_;
  } else {
    // Set stereo as a safe default
    pa_volume.channels = 2;
    spdlog::debug("Using default stereo volume structure");
  }

  // Set the volume safely
  volume = std::clamp(volume, min_volume, max_volume);
  pa_volume_t vol = volume * (static_cast<double>(PA_VOLUME_NORM) / 100);

  // Set all channels to the same volume manually to avoid pa_cvolume_set
  for (uint8_t i = 0; i < pa_volume.channels; i++) {
    pa_volume.values[i] = vol;
  }

  // Apply the volume change
  pa_threaded_mainloop_lock(mainloop_);
  pa_context_set_sink_volume_by_index(context_, sink_idx_, &pa_volume, volumeModifyCb, this);
  pa_threaded_mainloop_unlock(mainloop_);
}

void AudioBackend::changeVolume(ChangeType change_type, double step, uint16_t max_volume) {
  // Early return if context is not ready
  if ((context_ == nullptr) || pa_context_get_state(context_) != PA_CONTEXT_READY) {
    spdlog::error("PulseAudio context not ready");
    return;
  }

  // Prepare volume structure
  pa_cvolume pa_volume;
  pa_cvolume_init(&pa_volume);

  // Use existing volume structure if valid, otherwise create a safe default
  if ((pa_cvolume_valid(&pa_volume_) != 0) && (pa_channels_valid(pa_volume_.channels) != 0)) {
    pa_volume = pa_volume_;
  } else {
    // Set stereo as a safe default
    pa_volume.channels = 2;
    spdlog::debug("Using default stereo volume structure");

    // Initialize all channels to current volume level
    double volume_tick = static_cast<double>(PA_VOLUME_NORM) / 100;
    pa_volume_t vol = volume_ * volume_tick;
    for (uint8_t i = 0; i < pa_volume.channels; i++) {
      pa_volume.values[i] = vol;
    }

    // No need to continue with volume change if we had to create a new structure
    pa_threaded_mainloop_lock(mainloop_);
    pa_context_set_sink_volume_by_index(context_, sink_idx_, &pa_volume, volumeModifyCb, this);
    pa_threaded_mainloop_unlock(mainloop_);
    return;
  }

  // Calculate volume change
  double volume_tick = static_cast<double>(PA_VOLUME_NORM) / 100;
  pa_volume_t change;
  max_volume = std::min(max_volume, static_cast<uint16_t>(PA_VOLUME_UI_MAX));

  if (change_type == ChangeType::Increase && volume_ < max_volume) {
    // Calculate how much to increase
    if (volume_ + step > max_volume) {
      change = round((max_volume - volume_) * volume_tick);
    } else {
      change = round(step * volume_tick);
    }

    // Manually increase each channel's volume
    for (uint8_t i = 0; i < pa_volume.channels; i++) {
      pa_volume.values[i] = std::min(pa_volume.values[i] + change, PA_VOLUME_MAX);
    }
  } else if (change_type == ChangeType::Decrease && volume_ > 0) {
    // Calculate how much to decrease
    if (volume_ - step < 0) {
      change = round(volume_ * volume_tick);
    } else {
      change = round(step * volume_tick);
    }

    // Manually decrease each channel's volume
    for (uint8_t i = 0; i < pa_volume.channels; i++) {
      pa_volume.values[i] = (pa_volume.values[i] > change) ? (pa_volume.values[i] - change) : 0;
    }
  } else {
    // No change needed
    return;
  }

  // Apply the volume change
  pa_threaded_mainloop_lock(mainloop_);
  pa_context_set_sink_volume_by_index(context_, sink_idx_, &pa_volume, volumeModifyCb, this);
  pa_threaded_mainloop_unlock(mainloop_);
}

void AudioBackend::toggleSinkMute() {
  muted_ = !muted_;
  pa_threaded_mainloop_lock(mainloop_);
  pa_context_set_sink_mute_by_index(context_, sink_idx_, static_cast<int>(muted_), nullptr,
                                    nullptr);
  pa_threaded_mainloop_unlock(mainloop_);
}

void AudioBackend::toggleSinkMute(bool mute) {
  muted_ = mute;
  pa_threaded_mainloop_lock(mainloop_);
  pa_context_set_sink_mute_by_index(context_, sink_idx_, static_cast<int>(muted_), nullptr,
                                    nullptr);
  pa_threaded_mainloop_unlock(mainloop_);
}

void AudioBackend::toggleSourceMute() {
  source_muted_ = !muted_;
  pa_threaded_mainloop_lock(mainloop_);
  pa_context_set_source_mute_by_index(context_, source_idx_, static_cast<int>(source_muted_),
                                      nullptr, nullptr);
  pa_threaded_mainloop_unlock(mainloop_);
}

void AudioBackend::toggleSourceMute(bool mute) {
  source_muted_ = mute;
  pa_threaded_mainloop_lock(mainloop_);
  pa_context_set_source_mute_by_index(context_, source_idx_, static_cast<int>(source_muted_),
                                      nullptr, nullptr);
  pa_threaded_mainloop_unlock(mainloop_);
}

bool AudioBackend::isBluetooth() {
  return monitor_.find("a2dp_sink") != std::string::npos ||  // PulseAudio
         monitor_.find("a2dp-sink") != std::string::npos ||  // PipeWire
         monitor_.find("bluez") != std::string::npos;
}

void AudioBackend::setIgnoredSinks(const Json::Value &config) {
  if (config.isArray()) {
    for (const auto &ignored_sink : config) {
      if (ignored_sink.isString()) {
        ignored_sinks_.push_back(ignored_sink.asString());
      }
    }
  }
}

}  // namespace waybar::util
