// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixBackendRuntimeService.h"

#include "komai-rust-cxxbridge/ffi.h"
#include "matrix/backend/MatrixBackendBridge.h"
#include "matrix/backend/MatrixBackendRuntimeServiceInternal.h"
#include "matrix/backend/MatrixFfiBlockingContext.h"

namespace komai {

namespace {

MatrixRecoveryStatus
fromRustRecoveryStatus(const ::komai::rust::MatrixRecoveryStatus &status)
{
    return MatrixRecoveryStatus{
      .state                     = QString::fromStdString(std::string(status.state)),
      .hasDevicesToVerifyAgainst = status.has_devices_to_verify_against,
      .ownDeviceIsVerified       = status.own_device_is_verified,
      .hasUnverifiedOwnDevices   = status.has_unverified_own_devices,
    };
}

MatrixSetupRecoveryResult
fromRustSetupRecoveryResult(const ::komai::rust::MatrixSetupRecoveryResult &result)
{
    return MatrixSetupRecoveryResult{
      .recoveryKey = QString::fromStdString(std::string(result.recovery_key)),
    };
}

MatrixResetEncryptionIdentityResult
fromRustResetEncryptionIdentityResult(
  const ::komai::rust::MatrixResetEncryptionIdentityResult &result)
{
    return MatrixResetEncryptionIdentityResult{
      .completed   = result.completed,
      .authType    = QString::fromStdString(std::string(result.auth_type)),
      .approvalUrl = QString::fromStdString(std::string(result.approval_url)),
    };
}

MatrixDeviceSignOutResult
fromRustDeviceSignOutResult(const ::komai::rust::MatrixDeviceSignOutResult &result)
{
    return MatrixDeviceSignOutResult{
      .completed   = result.completed,
      .authType    = QString::fromStdString(std::string(result.auth_type)),
      .approvalUrl = QString::fromStdString(std::string(result.approval_url)),
    };
}

MatrixVerificationSession
fromRustVerificationSession(const ::komai::rust::MatrixVerificationSession &session)
{
    QVector<int> sasNumbers;
    sasNumbers.reserve(static_cast<int>(session.sas_numbers.size()));
    for (const auto number : session.sas_numbers)
        sasNumbers.push_back(static_cast<int>(number));

    return MatrixVerificationSession{
      .flowId                    = QString::fromStdString(std::string(session.flow_id)),
      .userId                    = QString::fromStdString(std::string(session.user_id)),
      .deviceId                  = QString::fromStdString(std::string(session.device_id)),
      .state                     = QString::fromStdString(std::string(session.state)),
      .error                     = QString::fromStdString(std::string(session.error)),
      .sender                    = session.sender,
      .isSelfVerification        = session.is_self_verification,
      .isMultiDeviceVerification = session.is_multi_device_verification,
      .sasNumbers                = sasNumbers,
    };
}

MatrixUserDevice
fromRustUserDevice(const ::komai::rust::MatrixUserDevice &device)
{
    return MatrixUserDevice{
      .deviceId          = QString::fromStdString(std::string(device.device_id)),
      .displayName       = QString::fromStdString(std::string(device.display_name)),
      .verificationState = QString::fromStdString(std::string(device.verification_state)),
      .lastIp            = QString::fromStdString(std::string(device.last_seen_ip)),
      .lastTs            = device.last_seen_ts,
    };
}

MatrixUserVerificationState
fromRustUserVerificationState(const ::komai::rust::MatrixUserVerificationState &state)
{
    QVector<MatrixUserDevice> devices;
    devices.reserve(static_cast<int>(state.devices.size()));
    for (const auto &device : state.devices)
        devices.push_back(fromRustUserDevice(device));

    return MatrixUserVerificationState{
      .hasMasterKey = state.has_master_key,
      .userTrust    = QString::fromStdString(std::string(state.user_trust)),
      .devices      = devices,
    };
}

} // anonymous namespace

std::optional<MatrixRecoveryStatus>
MatrixBackendRuntimeService::fetchRecoveryStatus(matrix_backend::BlockingCallContext context,
                                                 uint64_t handleId,
                                                 QString *errorOut)
{
    try {
        auto result =
          invokeRuntimeWorkerCall("matrix_fetch_recovery_status", [context, handleId]() {
              return ::komai::rust::matrix_fetch_recovery_status(
                matrix_backend::toRustBlockingContext(context), handleId);
          });
        return fromRustRecoveryStatus(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<uint64_t>
MatrixBackendRuntimeService::exportRoomKeys(matrix_backend::BlockingCallContext context,
                                            uint64_t handleId,
                                            const QString &path,
                                            const QString &passphrase,
                                            QString *errorOut)
{
    try {
        const auto count = invokeRuntimeWorkerCall(
          "matrix_export_room_keys", [context, handleId, path, passphrase]() {
              return ::komai::rust::matrix_export_room_keys(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                path.toStdString(),
                passphrase.toStdString());
          });
        return count;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixRoomKeyImportCounts>
MatrixBackendRuntimeService::importRoomKeys(matrix_backend::BlockingCallContext context,
                                            uint64_t handleId,
                                            const QString &path,
                                            const QString &passphrase,
                                            QString *errorOut)
{
    try {
        const auto result = invokeRuntimeWorkerCall(
          "matrix_import_room_keys", [context, handleId, path, passphrase]() {
              return ::komai::rust::matrix_import_room_keys(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                path.toStdString(),
                passphrase.toStdString());
          });
        return MatrixRoomKeyImportCounts{result.imported, result.total};
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixSetupRecoveryResult>
MatrixBackendRuntimeService::setupRecovery(matrix_backend::BlockingCallContext context,
                                           uint64_t handleId,
                                           bool useSSSS,
                                           const QString &passphrase,
                                           bool encryptionBackupOnlineEnabled,
                                           QString *errorOut)
{
    try {
        auto result = invokeRuntimeWorkerCall(
          "matrix_setup_recovery",
          [context, handleId, useSSSS, passphrase, encryptionBackupOnlineEnabled]() {
              return ::komai::rust::matrix_setup_recovery(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                useSSSS,
                passphrase.toStdString(),
                encryptionBackupOnlineEnabled);
          });
        return fromRustSetupRecoveryResult(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::recoverEncryptionSecrets(matrix_backend::BlockingCallContext context,
                                                      uint64_t handleId,
                                                      const QString &keyOrPassphrase,
                                                      QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_recover_encryption_secrets",
                                [context, handleId, keyOrPassphrase]() {
                                    ::komai::rust::matrix_recover_encryption_secrets(
                                      matrix_backend::toRustBlockingContext(context),
                                      handleId,
                                      keyOrPassphrase.toStdString());
                                });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixResetEncryptionIdentityResult>
MatrixBackendRuntimeService::startResetEncryptionIdentity(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  QString *errorOut)
{
    try {
        auto result =
          invokeRuntimeWorkerCall("matrix_start_reset_encryption_identity", [context, handleId]() {
              return ::komai::rust::matrix_start_reset_encryption_identity(
                matrix_backend::toRustBlockingContext(context), handleId);
          });
        return fromRustResetEncryptionIdentityResult(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::continueResetEncryptionIdentityWithPassword(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  const QString &password,
  QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall(
          "matrix_continue_reset_encryption_identity_with_password",
          [context, handleId, password]() {
              ::komai::rust::matrix_continue_reset_encryption_identity_with_password(
                matrix_backend::toRustBlockingContext(context), handleId, password.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::continueResetEncryptionIdentityAfterApproval(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall(
          "matrix_continue_reset_encryption_identity_after_approval", [context, handleId]() {
              ::komai::rust::matrix_continue_reset_encryption_identity_after_approval(
                matrix_backend::toRustBlockingContext(context), handleId);
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::cancelResetEncryptionIdentity(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_cancel_reset_encryption_identity", [context, handleId]() {
            ::komai::rust::matrix_cancel_reset_encryption_identity(
              matrix_backend::toRustBlockingContext(context), handleId);
        });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixDeviceSignOutResult>
MatrixBackendRuntimeService::startSignOutDevice(matrix_backend::BlockingCallContext context,
                                                uint64_t handleId,
                                                const QString &deviceId,
                                                QString *errorOut)
{
    try {
        auto result =
          invokeRuntimeWorkerCall("matrix_start_sign_out_device", [context, handleId, deviceId]() {
              return ::komai::rust::matrix_start_sign_out_device(
                matrix_backend::toRustBlockingContext(context), handleId, deviceId.toStdString());
          });
        return fromRustDeviceSignOutResult(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::continueSignOutDeviceWithPassword(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  const QString &password,
  QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall(
          "matrix_continue_sign_out_device_with_password", [context, handleId, password]() {
              ::komai::rust::matrix_continue_sign_out_device_with_password(
                matrix_backend::toRustBlockingContext(context), handleId, password.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::renameDevice(matrix_backend::BlockingCallContext context,
                                          uint64_t handleId,
                                          const QString &deviceId,
                                          const QString &displayName,
                                          QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall(
          "matrix_rename_device", [context, handleId, deviceId, displayName]() {
              ::komai::rust::matrix_rename_device(matrix_backend::toRustBlockingContext(context),
                                                  handleId,
                                                  deviceId.toStdString(),
                                                  displayName.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixVerificationSession>
MatrixBackendRuntimeService::startSelfVerification(matrix_backend::BlockingCallContext context,
                                                   uint64_t handleId,
                                                   QString *errorOut)
{
    try {
        auto result =
          invokeRuntimeWorkerCall("matrix_start_self_verification", [context, handleId]() {
              return ::komai::rust::matrix_start_self_verification(
                matrix_backend::toRustBlockingContext(context), handleId);
          });
        return fromRustVerificationSession(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixVerificationSession>
MatrixBackendRuntimeService::startUserVerification(matrix_backend::BlockingCallContext context,
                                                   uint64_t handleId,
                                                   const QString &userId,
                                                   QString *errorOut)
{
    try {
        auto result =
          invokeRuntimeWorkerCall("matrix_start_user_verification", [context, handleId, userId]() {
              return ::komai::rust::matrix_start_user_verification(
                matrix_backend::toRustBlockingContext(context), handleId, userId.toStdString());
          });
        return fromRustVerificationSession(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixVerificationSession>
MatrixBackendRuntimeService::startDeviceVerification(matrix_backend::BlockingCallContext context,
                                                     uint64_t handleId,
                                                     const QString &userId,
                                                     const QString &deviceId,
                                                     QString *errorOut)
{
    try {
        auto result = invokeRuntimeWorkerCall(
          "matrix_start_device_verification", [context, handleId, userId, deviceId]() {
              return ::komai::rust::matrix_start_device_verification(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                userId.toStdString(),
                deviceId.toStdString());
          });
        return fromRustVerificationSession(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::unverifyDevice(matrix_backend::BlockingCallContext context,
                                            uint64_t handleId,
                                            const QString &userId,
                                            const QString &deviceId,
                                            QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_unverify_device", [context, handleId, userId, deviceId]() {
            ::komai::rust::matrix_unverify_device(matrix_backend::toRustBlockingContext(context),
                                                  handleId,
                                                  userId.toStdString(),
                                                  deviceId.toStdString());
        });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::blockDevice(matrix_backend::BlockingCallContext context,
                                         uint64_t handleId,
                                         const QString &userId,
                                         const QString &deviceId,
                                         QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_block_device", [context, handleId, userId, deviceId]() {
            ::komai::rust::matrix_block_device(matrix_backend::toRustBlockingContext(context),
                                               handleId,
                                               userId.toStdString(),
                                               deviceId.toStdString());
        });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::unblockDevice(matrix_backend::BlockingCallContext context,
                                           uint64_t handleId,
                                           const QString &userId,
                                           const QString &deviceId,
                                           QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_unblock_device", [context, handleId, userId, deviceId]() {
            ::komai::rust::matrix_unblock_device(matrix_backend::toRustBlockingContext(context),
                                                 handleId,
                                                 userId.toStdString(),
                                                 deviceId.toStdString());
        });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixUserVerificationState>
MatrixBackendRuntimeService::fetchUserVerificationState(matrix_backend::BlockingCallContext context,
                                                        uint64_t handleId,
                                                        const QString &userId,
                                                        QString *errorOut)
{
    try {
        auto result = invokeRuntimeWorkerCall(
          "matrix_fetch_user_verification_state", [context, handleId, userId]() {
              return ::komai::rust::matrix_fetch_user_verification_state(
                matrix_backend::toRustBlockingContext(context), handleId, userId.toStdString());
          });
        return fromRustUserVerificationState(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QVector<QString>>
MatrixBackendRuntimeService::takePendingVerificationFlowIds(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  QString *errorOut)
{
    try {
        const auto result = invokeRuntimeWorkerCall(
          "matrix_take_pending_verification_flow_ids", [context, handleId]() {
              return ::komai::rust::matrix_take_pending_verification_flow_ids(
                matrix_backend::toRustBlockingContext(context), handleId);
          });
        QVector<QString> flowIds;
        flowIds.reserve(static_cast<int>(result.size()));
        for (const auto &flowId : result)
            flowIds.push_back(QString::fromStdString(std::string(flowId)));
        return flowIds;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixVerificationSession>
MatrixBackendRuntimeService::fetchVerificationSession(matrix_backend::BlockingCallContext context,
                                                      uint64_t handleId,
                                                      const QString &flowId,
                                                      QString *errorOut)
{
    try {
        auto result = invokeRuntimeWorkerCall(
          "matrix_fetch_verification_session", [context, handleId, flowId]() {
              return ::komai::rust::matrix_fetch_verification_session(
                matrix_backend::toRustBlockingContext(context), handleId, flowId.toStdString());
          });
        return fromRustVerificationSession(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::clearVerificationSession(matrix_backend::BlockingCallContext context,
                                                      uint64_t handleId,
                                                      const QString &flowId,
                                                      QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_clear_verification_session", [context, handleId, flowId]() {
            ::komai::rust::matrix_clear_verification_session(
              matrix_backend::toRustBlockingContext(context), handleId, flowId.toStdString());
        });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::advanceVerificationSession(matrix_backend::BlockingCallContext context,
                                                        uint64_t handleId,
                                                        const QString &flowId,
                                                        QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall(
          "matrix_advance_verification_session", [context, handleId, flowId]() {
              ::komai::rust::matrix_advance_verification_session(
                matrix_backend::toRustBlockingContext(context), handleId, flowId.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::cancelVerificationSession(matrix_backend::BlockingCallContext context,
                                                       uint64_t handleId,
                                                       const QString &flowId,
                                                       bool mismatch,
                                                       QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_cancel_verification_session",
                                [context, handleId, flowId, mismatch]() {
                                    ::komai::rust::matrix_cancel_verification_session(
                                      matrix_backend::toRustBlockingContext(context),
                                      handleId,
                                      flowId.toStdString(),
                                      mismatch);
                                });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

} // namespace komai
