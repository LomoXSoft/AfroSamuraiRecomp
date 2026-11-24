// Download and hash helpers for the graphical setup (Windows only).
//
// Since 0.3.10 the setup exe carries no source files. They are a separate
// release asset (afrosetup-v<version>-files.bin), and the setup accepts them
// only when their SHA-256 equals the one compiled into the exe.
#pragma once

#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>

#include <cstdint>
#include <string>
#include <vector>

namespace afrosetup {

// SHA-256 of `data` as 64 lowercase hex digits, or "" if hashing failed.
inline std::string Sha256Hex(const uint8_t* data, size_t n) {
  BCRYPT_ALG_HANDLE alg = nullptr;
  if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
  BCRYPT_HASH_HANDLE hash = nullptr;
  uint8_t digest[32] = {};
  bool ok = BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
  size_t pos = 0;
  while (ok && pos < n) {
    const ULONG chunk = static_cast<ULONG>(n - pos > (1u << 30) ? (1u << 30) : n - pos);
    ok = BCryptHashData(hash, const_cast<PUCHAR>(data + pos), chunk, 0) >= 0;
    pos += chunk;
  }
  ok = ok && BCryptFinishHash(hash, digest, sizeof digest, 0) >= 0;
  if (hash) BCryptDestroyHash(hash);
  BCryptCloseAlgorithmProvider(alg, 0);
  if (!ok) return {};
  static const char kHex[] = "0123456789abcdef";
  std::string out;
  for (uint8_t b : digest) {
    out.push_back(kHex[b >> 4]);
    out.push_back(kHex[b & 15]);
  }
  return out;
}

// HTTPS GET of https://<host><path> into `out`. Redirects are followed (GitHub
// serves release assets from another host). Fails on any status but 200 and
// on bodies over `max_bytes`. `error` gets a short English reason.
inline bool HttpsGet(const wchar_t* host, const std::wstring& path, size_t max_bytes, std::vector<uint8_t>& out,
                     std::wstring& error) {
  out.clear();
  const std::wstring agent = std::wstring(L"AfroSamuraiRecomp-Setup");
  HINTERNET session = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                  WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session)
    session = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                          WINHTTP_NO_PROXY_BYPASS, 0);
  HINTERNET connect = nullptr, request = nullptr;
  DWORD fail = 0;
  const wchar_t* stage = L"open";
  bool ok = false;
  do {
    if (!session) break;
    WinHttpSetTimeouts(session, 30000, 30000, 30000, 60000);
    stage = L"connect";
    connect = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) break;
    request = WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                 WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request) break;
    stage = L"request";
    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) break;
    if (!WinHttpReceiveResponse(request, nullptr)) break;
    DWORD status = 0, len = sizeof status;
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                             &status, &len, WINHTTP_NO_HEADER_INDEX))
      break;
    if (status != 200) {
      error = L"HTTP " + std::to_wstring(status);
      stage = nullptr;
      break;
    }
    stage = L"download";
    for (;;) {
      DWORD avail = 0;
      if (!WinHttpQueryDataAvailable(request, &avail)) break;
      if (avail == 0) {
        ok = true;
        break;
      }
      if (out.size() + avail > max_bytes) {
        error = L"the file is larger than expected";
        stage = nullptr;
        break;
      }
      const size_t at = out.size();
      out.resize(at + avail);
      DWORD got = 0;
      if (!WinHttpReadData(request, out.data() + at, avail, &got)) break;
      out.resize(at + got);
    }
  } while (false);
  if (!ok && stage) {
    fail = GetLastError();
    error = std::wstring(stage) + L" failed (WinHTTP error " + std::to_wstring(fail) + L")";
  }
  if (request) WinHttpCloseHandle(request);
  if (connect) WinHttpCloseHandle(connect);
  if (session) WinHttpCloseHandle(session);
  if (!ok) out.clear();
  return ok;
}

}  // namespace afrosetup
