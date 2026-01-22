# inventory - module containing Ansible inventory handling primitives
#
# Copyright (C) 2017-2020 Red Hat, Inc.
#
# SPDX-License-Identifier: GPL-2.0-or-later

import logging

from pathlib import Path

from lcitool import util, LcitoolError
from lcitool.packages import package_names_by_type
from lcitool.config import Config
from lcitool.projects import Projects
from lcitool.targets import BuildTarget, Targets
from typing import Any, Dict, List, Optional, Union

log = logging.getLogger(__name__)


class InventoryError(LcitoolError):
    """Global exception type for the inventory module."""

    def __init__(self, message: str):
        super().__init__(message, "Inventory")


class Inventory:

    def __init__(
        self,
        targets: Targets,
        config: Config,
        inventory_path: Optional[Path] = None,
    ):
        self._targets = targets
        self._config = config
        self._host_facts: Optional[Dict[str, Dict[str, Any]]] = None
        self._ansible_inventory: Optional[Dict[str, Dict[str, Any]]] = None
        self._inventory_path = inventory_path

    @property
    def ansible_inventory(
        self,
    ) -> Dict[
        str,
        Dict[str, Any],
    ]:
        if self._ansible_inventory is None:
            self._ansible_inventory = self._get_ansible_inventory()
        assert isinstance(self._ansible_inventory, dict)
        return self._ansible_inventory

    @property
    def host_facts(self) -> Dict[str, Dict[str, Any]]:
        if self._host_facts is None:
            self._host_facts = self._load_host_facts()
        assert isinstance(self._host_facts, dict)
        return self._host_facts

    @property
    def hosts(self) -> List[str]:
        return list(self.host_facts.keys())

    def _get_ansible_inventory(
        self,
    ) -> Any:
        from lcitool.ansible_wrapper import AnsibleWrapper, AnsibleWrapperError

        inventory_sources: List[Union[Path, Dict[str, Any]]] = []
        if self._inventory_path is None:
            self._inventory_path = Path(util.get_config_dir(), "inventory")

            # we only call into libvirt when we need to use default inventory
            # sources, i.e. user didn't provide one via datadir
            log.debug("Querying libvirt for lcitool hosts")
            inventory_sources.append(self._get_libvirt_inventory())

        if self._inventory_path.exists():
            log.debug(f"Adding '{self._inventory_path}' to Ansible inventory sources")
            inventory_sources.append(self._inventory_path)

        ansible_runner = AnsibleWrapper()
        ansible_runner.prepare_env(
            inventories=inventory_sources, group_vars=self._targets.target_facts
        )

        log.debug(f"Running ansible-inventory on '{inventory_sources}'")
        try:
            inventory = ansible_runner.get_inventory()
        except AnsibleWrapperError as ex:
            log.debug("Failed to load Ansible inventory")
            raise InventoryError(f"Failed to load Ansible inventory: {ex}")

        return inventory

    def _get_libvirt_inventory(self) -> Dict[str, Any]:
        from lcitool.libvirt_wrapper import LibvirtWrapper

        inventory: Dict[str, Any] = {"all": {"children": {}}}
        children = inventory["all"]["children"]

        for host, target in LibvirtWrapper().hosts.items():
            inventory_target = children.setdefault(target, {})
            inventory_hosts = inventory_target.setdefault("hosts", {})
            inventory_hosts.setdefault(host, {})

        return inventory

    def _load_host_facts(
        self,
    ) -> Dict[str, Dict[str, Any]]:
        facts: Dict[str, Dict[str, Any]] = {}
        groups: Dict[str, Any] = {}

        def _rec(inventory: Dict[str, Any], group_name: str) -> None:
            for key, subinventory in inventory.items():
                if key == "hosts":
                    if (
                        group_name != "ungrouped"
                        and group_name not in self._targets.targets
                    ):
                        log.info(
                            f"Unsupported target OS group '{group_name}'"
                            "found in the inventory, skipping..."
                        )
                        return

                    for host_name, host_facts in subinventory.items():
                        log.debug(f"Host '{host_name}' is in group '{group_name}'")

                        # Keep track of all the groups we've seen each host
                        # show up in so that we can perform some validation
                        # later
                        if host_name not in groups:
                            groups[host_name] = set()
                        groups[host_name].add(group_name)

                        # ansible-inventory only includes the full list of facts
                        # the first time a host shows up, no matter how deeply
                        # nested that happens to be, and all other times just uses
                        # an empty dictionary as a position marker
                        if host_name not in facts:
                            log.debug(f"Facts for host '{host_name}': {host_facts}")
                            facts[host_name] = host_facts

                # Recurse into the group's children to look for more hosts
                elif key == "children":
                    _rec(subinventory, group_name)
                else:
                    log.debug(f"Group '{key}' is a children of group '{group_name}'")
                    _rec(subinventory, key)

        _rec(self.ansible_inventory["all"], "all")

        targets = set(self._targets.targets)
        for host_name, host_groups in groups.items():
            host_targets = host_groups.intersection(targets)

            # Each host should have shown up in exactly one of the groups
            # that are defined based on the target OS
            if len(host_targets) == 0:
                raise InventoryError(
                    f"Host '{host_name}' not found in any target OS group"
                )
            elif len(host_targets) > 1:
                raise InventoryError(
                    f"Host '{host_name}' found in multiple target OS groups: {host_targets}"
                )

        return facts

    def expand_hosts(self, pattern: str) -> List[str]:
        try:
            return util.expand_pattern(pattern, self.hosts, "hosts")
        except InventoryError as ex:
            raise ex
        except Exception as ex:
            log.debug(f"Failed to load expand '{pattern}'")
            raise InventoryError(f"Failed to expand '{pattern}': {ex}")

    def get_host_target_name(self, host: str) -> str:
        target = self.host_facts[host]["target"]
        assert isinstance(target, str)
        return target

    def get_group_vars(
        self, target: BuildTarget, projects: Projects, projects_expanded: List[str]
    ) -> Dict[str, Union[Dict[str, str], str, List[str]]]:
        # resolve the package mappings to actual package names
        internal_wanted_projects = ["base", "developer", "vm"]
        if self._config.values["install"]["cloud_init"]:
            internal_wanted_projects.append("cloud-init")

        selected_projects = internal_wanted_projects + projects_expanded
        pkgs_install = projects.get_packages(selected_projects, target)
        pkgs_early_install = projects.get_packages(["early_install"], target)
        pkgs_remove = projects.get_packages(["unwanted"], target)
        package_names = package_names_by_type(pkgs_install)
        package_names_remove = package_names_by_type(pkgs_remove)
        package_names_early_install = package_names_by_type(pkgs_early_install)

        # merge the package lists to the Ansible group vars
        group_vars = dict(target.facts)
        group_vars["packages"] = package_names["native"]
        group_vars["pypi_packages"] = package_names["pypi"]
        group_vars["cpan_packages"] = package_names["cpan"]
        group_vars["unwanted_packages"] = package_names_remove["native"]
        group_vars["early_install_packages"] = package_names_early_install["native"]
        return group_vars
