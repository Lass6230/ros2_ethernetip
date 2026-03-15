#include "ethernetip_master/explicit_messaging.hpp"

#include <rclcpp/rclcpp.hpp>

#include <SessionInfo.h>
#include <MessageRouter.h>
#include <cip/Services.h>
#include <cip/GeneralStatusCodes.h>
#include <cip/EPath.h>

namespace ethernetip_master
{

struct ExplicitMessaging::Impl
{
  std::shared_ptr<eipScanner::SessionInfo> session;
  eipScanner::MessageRouter router;
};

ExplicitMessaging::ExplicitMessaging() = default;
ExplicitMessaging::~ExplicitMessaging() = default;

bool ExplicitMessaging::openSession(const std::string & ip)
{
  try {
    impl_ = std::make_unique<Impl>();
    impl_->session = std::make_shared<eipScanner::SessionInfo>(ip, 0xAF12);
    connected_ = true;
    return true;
  } catch (const std::exception & ex) {
    RCLCPP_ERROR(
      rclcpp::get_logger("ExplicitMessaging"),
      "Failed to open session to %s: %s", ip.c_str(), ex.what());
    connected_ = false;
    return false;
  }
}

void ExplicitMessaging::closeSession()
{
  impl_.reset();
  connected_ = false;
}

std::vector<uint8_t> ExplicitMessaging::readAttribute(
  uint16_t class_id, uint16_t instance_id, uint16_t attribute_id)
{
  if (!connected_ || !impl_) {
    return {};
  }

  try {
    eipScanner::cip::EPath path(class_id, instance_id, attribute_id);

    auto response = impl_->router.sendRequest(
      impl_->session,
      eipScanner::cip::ServiceCodes::GET_ATTRIBUTE_SINGLE,
      path,
      {});

    if (response.getGeneralStatusCode() ==
        eipScanner::cip::GeneralStatusCodes::SUCCESS)
    {
      return response.getData();
    } else {
      RCLCPP_WARN(
        rclcpp::get_logger("ExplicitMessaging"),
        "GetAttributeSingle failed: status 0x%02X",
        static_cast<int>(response.getGeneralStatusCode()));
    }
  } catch (const std::exception & ex) {
    RCLCPP_ERROR(
      rclcpp::get_logger("ExplicitMessaging"),
      "readAttribute exception: %s", ex.what());
  }
  return {};
}

bool ExplicitMessaging::writeAttribute(
  uint16_t class_id, uint16_t instance_id, uint16_t attribute_id,
  const std::vector<uint8_t> & data)
{
  if (!connected_ || !impl_) {
    return false;
  }

  try {
    eipScanner::cip::EPath path(class_id, instance_id, attribute_id);

    auto response = impl_->router.sendRequest(
      impl_->session,
      eipScanner::cip::ServiceCodes::SET_ATTRIBUTE_SINGLE,
      path,
      data);

    if (response.getGeneralStatusCode() ==
        eipScanner::cip::GeneralStatusCodes::SUCCESS)
    {
      return true;
    }
    RCLCPP_WARN(
      rclcpp::get_logger("ExplicitMessaging"),
      "SetAttributeSingle failed: status 0x%02X",
      static_cast<int>(response.getGeneralStatusCode()));
  } catch (const std::exception & ex) {
    RCLCPP_ERROR(
      rclcpp::get_logger("ExplicitMessaging"),
      "writeAttribute exception: %s", ex.what());
  }
  return false;
}

std::string ExplicitMessaging::readIdentity()
{
  // Identity Object: Class 0x01, Instance 1, Attribute 7 = Product Name
  auto data = readAttribute(0x01, 1, 7);
  if (data.empty()) {
    return "<unknown>";
  }
  // CIP short string: first byte = length, then chars
  uint8_t len = data[0];
  if (data.size() < static_cast<std::size_t>(len + 1)) {
    return "<malformed>";
  }
  return std::string(reinterpret_cast<const char *>(data.data() + 1), len);
}

}  // namespace ethernetip_master
