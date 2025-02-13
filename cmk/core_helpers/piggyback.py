#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (C) 2019 tribe29 GmbH - License: GNU General Public License v2
# This file is part of Checkmk (https://checkmk.com). It is subject to the terms and
# conditions defined in the file COPYING, which is part of this source code package.

import itertools
import json
import logging
from typing import Any, Dict, Final, List, Optional, Sequence, Tuple

from cmk.utils.piggyback import get_piggyback_raw_data, PiggybackRawDataInfo, PiggybackTimeSettings
from cmk.utils.type_defs import (
    AgentRawData,
    ExitSpec,
    HostAddress,
    HostName,
    ServiceDetails,
    ServiceState,
)

from .agent import AgentFetcher, AgentHostSections, AgentSummarizer, NoCache
from .type_defs import Mode


class PiggybackFetcher(AgentFetcher):
    def __init__(
        self,
        file_cache: NoCache,
        *,
        cluster_nodes: Sequence[HostName],
        hostname: HostName,
        address: Optional[HostAddress],
        time_settings: List[Tuple[Optional[str], str, int]],
    ) -> None:
        super().__init__(
            file_cache,
            cluster_nodes,
            logging.getLogger("cmk.helper.piggyback"),
        )
        self.hostname: Final = hostname
        self.address: Final = address
        self.time_settings: Final = time_settings
        self._sources: List[PiggybackRawDataInfo] = []

    @classmethod
    def _from_json(cls, serialized: Dict[str, Any]) -> "PiggybackFetcher":
        return cls(
            NoCache.from_json(serialized.pop("file_cache")),
            **serialized,
        )

    def to_json(self) -> Dict[str, Any]:
        return {
            "file_cache": self.file_cache.to_json(),
            "cluster_nodes": self.cluster_nodes,
            "hostname": self.hostname,
            "address": self.address,
            "time_settings": self.time_settings,
        }

    def open(self) -> None:
        for origin in (self.hostname, self.address):
            self._sources.extend(PiggybackFetcher._raw_data(origin, self.time_settings))

    def close(self) -> None:
        self._sources.clear()

    def _fetch_from_io(self, mode: Mode) -> AgentRawData:
        return AgentRawData(b"" + self._get_main_section() + self._get_source_labels_section())

    def _get_main_section(self) -> AgentRawData:
        raw_data = AgentRawData(b"")
        for src in self._sources:
            if src.successfully_processed:
                # !! Important for Check_MK and Check_MK Discovery service !!
                #   - sources contains ALL file infos and is not filtered
                #     in cmk/base/piggyback.py as in previous versions
                #   - Check_MK gets the processed file info reasons and displays them in
                #     it's service details
                #   - Check_MK Discovery: Only shows vanished/new/... if raw data is not
                #     added; ie. if file_info is not successfully processed
                raw_data = AgentRawData(raw_data + src.raw_data)
        return raw_data

    def _get_source_labels_section(self) -> AgentRawData:
        """Return a <<<labels>>> agent section which adds the piggyback sources
        to the labels of the current host"""
        if not self._sources:
            return AgentRawData(b"")

        labels = {"cmk/piggyback_source_%s" % src.source_hostname: "yes" for src in self._sources}
        return AgentRawData(b'<<<labels:sep(0)>>>\n%s\n' % json.dumps(labels).encode("utf-8"))

    @staticmethod
    def _raw_data(
        hostname: Optional[str],
        time_settings: PiggybackTimeSettings,
    ) -> List[PiggybackRawDataInfo]:
        return get_piggyback_raw_data(hostname if hostname else "", time_settings)


class PiggybackSummarizer(AgentSummarizer):
    def __init__(
        self,
        exit_spec: ExitSpec,
        *,
        hostname: HostName,
        ipaddress: Optional[HostAddress],
        time_settings: List[Tuple[Optional[str], str, int]],
        always: bool,
    ) -> None:
        super().__init__(exit_spec)
        self.hostname = hostname
        self.ipaddress = ipaddress
        self.time_settings = time_settings
        self.always = always

    def __repr__(self) -> str:
        return "%s(%r, hostname=%r, ipaddress=%r, time_settings=%r, always=%r)" % (
            type(self).__name__,
            self.exit_spec,
            self.hostname,
            self.ipaddress,
            self.time_settings,
            self.always,
        )

    def summarize_success(
        self,
        host_sections: AgentHostSections,
        *,
        mode: Mode,
    ) -> Tuple[ServiceState, ServiceDetails]:
        """Returns useful information about the data source execution

        Return only summary information in case there is piggyback data"""
        if mode is not Mode.CHECKING:
            return 0, ''

        sources: Final[Sequence[PiggybackRawDataInfo]] = list(
            itertools.chain.from_iterable(
                # TODO(ml): The code uses `get_piggyback_raw_data()` instead of
                # `HostSections.piggyback_raw_data` because this allows it to
                # sneakily use cached data.  At minimum, we should group all cache
                # handling performed after the parser.
                get_piggyback_raw_data(origin, self.time_settings)
                for origin in (self.hostname, self.ipaddress)))
        if not sources:
            if self.always:
                return 1, "Missing data"
            return 0, ''
        return (
            max(src.reason_status for src in sources),
            ", ".join(src.reason for src in sources if src.reason),
        )
