// This file is a part of the IncludeOS unikernel - www.includeos.org
//
// Copyright 2015-2016 Oslo and Akershus University College of Applied Sciences
// and Alfred Bratterud
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <net/http/message.hpp>

namespace http {

///////////////////////////////////////////////////////////////////////////////
Message::Message(const std::size_t limit) noexcept
  : header_fields_{limit}
{}

///////////////////////////////////////////////////////////////////////////////
Header& Message::header() noexcept {
  return header_fields_;
}

///////////////////////////////////////////////////////////////////////////////
const Header& Message::header() const noexcept {
  return header_fields_;
}

///////////////////////////////////////////////////////////////////////////////
Message& Message::add_body(const Message_body& message_body) {
  if (message_body.empty()) return *this;
  //-----------------------------------
  message_body_   = message_body;
  content_length_ = std::to_string(message_body_.size());
  //-----------------------------------
  header().add_field(header::Content_Length, content_length_);
  return *this;
}

///////////////////////////////////////////////////////////////////////////////
Message& Message::add_chunk(const std::string& chunk) {
  if (chunk.empty()) return *this;
  //-----------------------------------
  message_body_.append(chunk);
  content_length_ = std::to_string(message_body_.size());
  //-----------------------------------
  header().set_field(header::Content_Length, content_length_);
  return *this;
}

///////////////////////////////////////////////////////////////////////////////
bool Message::has_body() const noexcept {
  return not message_body_.empty();
}

///////////////////////////////////////////////////////////////////////////////
std::experimental::string_view Message::body() const noexcept {
  return message_body_;
}

///////////////////////////////////////////////////////////////////////////////
Message& Message::clear_body() noexcept {
  message_body_.clear();
  header().erase(header::Content_Length);
  return *this;
}

///////////////////////////////////////////////////////////////////////////////
Message& Message::reset() noexcept {
  header().clear();
  return clear_body();
}

///////////////////////////////////////////////////////////////////////////////
std::string Message::to_string() const {
  std::ostringstream message;
  //-----------------------------------
  message << header_fields_
          << message_body_;
  //-----------------------------------
  return message.str();
}

///////////////////////////////////////////////////////////////////////////////
Message::operator std::string () const {
  return to_string();
}

///////////////////////////////////////////////////////////////////////////////
const std::experimental::string_view Message::private_field() const noexcept {
  return field_;
}

///////////////////////////////////////////////////////////////////////////////
void Message::set_private_field(const char* base, const size_t length) noexcept {
  field_ = {base, length};
}

///////////////////////////////////////////////////////////////////////////////
std::ostream& operator << (std::ostream& output_device, const Message& message) {
  return output_device << message.to_string();
}

} //< namespace http
