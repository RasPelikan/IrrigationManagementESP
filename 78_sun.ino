// Sunrise / sunset calculation for daylight-gated well-pump scheduling.
//
// Algorithm: USNO "Almanac for Computers" (1990) — small, well-known, accurate
// to ±1 minute for civil use down to ~60° latitude, which is comfortably wider
// than this device will ever be deployed. No external API call.
//
// The result is cached and recomputed only when the local day-of-year changes,
// so calling recomputeSunTimes() every minute is essentially free.
//
// Stored as local minutes from midnight (0..1439). -1 means: not yet computed
// (NTP still pending), or sun never rises/sets (polar regions). In both
// degenerate cases isInDaylightWindow() falls open — the daylight constraint
// must never silently keep the pump permanently off without telling the user.

int16_t sunriseLocalMin = -1;
int16_t sunsetLocalMin = -1;
int16_t sunComputedYday = -1;
int16_t sunComputedYear = -1;
// tm_isdst is part of the cache key so that the spring/autumn DST transition
// (which happens mid-day, at 02:00/03:00 local on the last Sunday of Mar/Oct)
// triggers a recompute with the new UTC offset. Otherwise the values cached
// at midnight under the old offset would be off by 1h until the next day.
int8_t sunComputedIsDst = -1;

// USNO Almanac §A. `rise=true` → sunrise; `rise=false` → sunset.
// Returns UTC hours in [0, 24), or -1.0 if the sun never crosses the zenith.
static double computeSunRiseOrSet(uint16_t year, uint8_t month, uint8_t day,
                                  float lat, float lon, bool rise) {

  int N1 = 275 * month / 9;
  int N2 = (month + 9) / 12;
  int N3 = 1 + (year - 4 * (year / 4) + 2) / 3;
  double N = (double)(N1 - (N2 * N3) + day - 30);

  double lngHour = lon / 15.0;
  double t = rise ? N + (6.0 - lngHour) / 24.0
                  : N + (18.0 - lngHour) / 24.0;

  double M = (0.9856 * t) - 3.289;

  double Mrad = M * DEG_TO_RAD;
  double L = M + 1.916 * sin(Mrad) + 0.020 * sin(2.0 * Mrad) + 282.634;
  while (L >= 360.0) L -= 360.0;
  while (L < 0.0) L += 360.0;
  double Lrad = L * DEG_TO_RAD;

  // Sun's right ascension, forced into the same quadrant as L (per USNO §5b).
  double RA = atan(0.91764 * tan(Lrad)) * RAD_TO_DEG;
  while (RA >= 360.0) RA -= 360.0;
  while (RA < 0.0) RA += 360.0;
  double Lq = floor(L / 90.0) * 90.0;
  double RAq = floor(RA / 90.0) * 90.0;
  RA = (RA + (Lq - RAq)) / 15.0;

  double sinDec = 0.39782 * sin(Lrad);
  double cosDec = cos(asin(sinDec));

  // 90.833° = 90° geometric + ~50' for atmospheric refraction at the horizon.
  double zenithRad = 90.833 * DEG_TO_RAD;
  double latRad = lat * DEG_TO_RAD;
  double cosH = (cos(zenithRad) - sinDec * sin(latRad)) / (cosDec * cos(latRad));

  if (cosH > 1.0 || cosH < -1.0) {
    return -1.0; // sun never rises (cosH>1) or never sets (cosH<-1)
  }
  double H = rise ? 360.0 - acos(cosH) * RAD_TO_DEG
                  : acos(cosH) * RAD_TO_DEG;
  H /= 15.0;

  double T = H + RA - 0.06571 * t - 6.622;
  double UT = T - lngHour;
  while (UT < 0.0) UT += 24.0;
  while (UT >= 24.0) UT -= 24.0;
  return UT;

}

// Recompute sunrise/sunset for today's local date. No-op on subsequent calls
// within the same day, so it's cheap to call every minute.
void recomputeSunTimes() {

  if (!irrigationConfig.daylightEnabled) return;
  if (now == 0) return; // NTP not synced yet

  if (sunComputedYday == tm_now.tm_yday
      && sunComputedYear == tm_now.tm_year
      && sunComputedIsDst == tm_now.tm_isdst) {
    return;
  }

  uint16_t year = tm_now.tm_year + 1900;
  uint8_t month = tm_now.tm_mon + 1;
  uint8_t day = tm_now.tm_mday;
  // ESP32's newlib does NOT expose `tm_gmtoff`, so we derive the local-UTC
  // offset (seconds east of UTC, DST included) from the diff between
  // localtime and gmtime of the same epoch second. The dayDiff term handles
  // the edge case where local time has already crossed midnight relative to
  // UTC (or vice-versa) — for CET that's harmless, but it's correct in
  // general so future relocations don't bite.
  tm utc_tm;
  gmtime_r(&now, &utc_tm);
  long localSec = (long)tm_now.tm_hour * 3600L + tm_now.tm_min * 60L + tm_now.tm_sec;
  long utcSec   = (long)utc_tm.tm_hour * 3600L + utc_tm.tm_min * 60L + utc_tm.tm_sec;
  long gmtOffsetSec = localSec - utcSec;
  int dayDiff;
  if (tm_now.tm_year == utc_tm.tm_year) {
    dayDiff = tm_now.tm_yday - utc_tm.tm_yday;
  } else {
    dayDiff = (tm_now.tm_year > utc_tm.tm_year) ? 1 : -1;
  }
  gmtOffsetSec += (long)dayDiff * 86400L;

  double sunriseUtc = computeSunRiseOrSet(
      year, month, day,
      irrigationConfig.locationLatitude, irrigationConfig.locationLongitude,
      true);
  double sunsetUtc = computeSunRiseOrSet(
      year, month, day,
      irrigationConfig.locationLatitude, irrigationConfig.locationLongitude,
      false);

  auto toLocalMin = [&](double utcHours) -> int16_t {
    if (utcHours < 0.0) return -1;
    long localMin = (long)(utcHours * 60.0 + 0.5) + gmtOffsetSec / 60;
    while (localMin < 0) localMin += 1440;
    while (localMin >= 1440) localMin -= 1440;
    return (int16_t)localMin;
  };

  sunriseLocalMin = toLocalMin(sunriseUtc);
  sunsetLocalMin = toLocalMin(sunsetUtc);
  sunComputedYday = tm_now.tm_yday;
  sunComputedYear = tm_now.tm_year;
  sunComputedIsDst = tm_now.tm_isdst;

  Serial.printf(
      "Sun for %04u-%02u-%02u @ (%.4f, %.4f): sunrise=%02d:%02d sunset=%02d:%02d local\n",
      year, month, day,
      irrigationConfig.locationLatitude, irrigationConfig.locationLongitude,
      sunriseLocalMin >= 0 ? sunriseLocalMin / 60 : -1,
      sunriseLocalMin >= 0 ? sunriseLocalMin % 60 : 0,
      sunsetLocalMin >= 0 ? sunsetLocalMin / 60 : -1,
      sunsetLocalMin >= 0 ? sunsetLocalMin % 60 : 0);

  updateStatusClients(STATUS_UPDATE_WELLPUMP);

}

// True iff the current local minute lies inside [sunrise+startOffset, sunset-endOffset).
// Fails OPEN for safety reasons:
//   - daylight feature disabled → always true (no constraint)
//   - NTP not yet synced (sun times still -1) → always true; the pump should
//     keep working in its old 45/15 cycle until time is known
//   - polar day / polar night (sun never rises/sets) → always true
// If the user configures offsets so wide that the window becomes empty, we
// return false (user clearly intends to disable the pump that way).
bool isInDaylightWindow() {

  if (!irrigationConfig.daylightEnabled) return true;
  if (sunriseLocalMin < 0 || sunsetLocalMin < 0) return true;

  int currentMin = tm_now.tm_hour * 60 + tm_now.tm_min;
  int windowStart = sunriseLocalMin + irrigationConfig.wellPumpDaylightStartOffsetMin;
  int windowEnd = sunsetLocalMin - irrigationConfig.wellPumpDaylightEndOffsetMin;

  if (windowStart >= windowEnd) return false;
  return currentMin >= windowStart && currentMin < windowEnd;

}
