#ifndef ETHERNETIP_MASTER__EXPLICIT_MESSAGING_HPP_
#define ETHERNETIP_MASTER__EXPLICIT_MESSAGING_HPP_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ethernetip_master
{

/// Thin wrapper around EIPScanner explicit‑message services (Get/Set
/// Attribute Single, etc.).
class ExplicitMessaging
{
public:
  ExplicitMessaging();
  ~ExplicitMessaging();

  /// Open a TCP session to \p ip.
  bool openSession(const std::string & ip);

  /// Close the session.
  void closeSession();

  /// CIP Get Attribute Single.
  std::vector<uint8_t> readAttribute(
    uint16_t class_id, uint16_t instance_id, uint16_t attribute_id);

  /// CIP Set Attribute Single.
  bool writeAttribute(
    uint16_t class_id, uint16_t instance_id, uint16_t attribute_id,
    const std::vector<uint8_t> & data);

  /// Read the Identity Object (class 0x01, instance 1) and return a
  /// human‑readable summary string.
  std::string readIdentity();

  /// Return true when a TCP session is active.
  bool isConnected() const { return connected_; }

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  bool connected_{false};
};

}  // namespace ethernetip_master

#endif  // ETHERNETIP_MASTER__EXPLICIT_MESSAGING_HPP_
