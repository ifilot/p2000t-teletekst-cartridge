#!/usr/bin/env python3
"""@file build_info.py
@brief Generate cartridge build metadata in Dutch local time.

SPDX-License-Identifier: GPL-3.0-only
"""

from datetime import datetime, timezone
from zoneinfo import ZoneInfo
import json


def build_timestamp(instant: datetime | None = None) -> str:
    """@brief Format a build instant using Amsterdam's daylight-saving rules.

    @param[in] instant A timezone-aware instant, or None for the current time.
    @return Local date and time with its CET or CEST abbreviation.
    """
    instant = instant if instant is not None else datetime.now(timezone.utc)
    return instant.astimezone(ZoneInfo("Europe/Amsterdam")).strftime(
        "%d-%m-%Y %H:%M %Z"
    )


if __name__ == "__main__":
    print("#define P2WP_BUILD_TIMESTAMP " + json.dumps(build_timestamp()))
