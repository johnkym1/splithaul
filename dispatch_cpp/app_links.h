#pragma once

namespace appLinks {
// Public donor address, supplied by Grumpyman. Do not link to the owner dashboard.
inline constexpr wchar_t donationUrl[] = L"https://github.com/sponsors/johnkym1";
// The Sponsors dashboard still reported pending approval on 23 November 2026.
// Enable after the Sponsors page is publicly active and has been checked.
inline constexpr bool donationPageActive = false;
inline constexpr bool donationAvailable = donationPageActive && donationUrl[0] != L'\0';
}
