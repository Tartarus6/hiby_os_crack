# docker.py - module implementing docker container runtime wrapper logic
#
# Copyright (c) 2023 Abdulwasiu Apalowo.
#
# SPDX-License-Identifier: GPL-2.0-or-later

import json
import logging

from typing import Any, Union, Optional, List
from pathlib import Path

from .containers import Container

log = logging.getLogger()


class Docker(Container):
    """Docker container class"""

    def run(
        self,
        image: str,
        container_cmd: str,
        user: Union[str, int],
        tempdir: Path,
        env: Optional[List[str]] = None,
        datadir: Optional[Path] = None,
        script: Optional[Path] = None,
        **kwargs: Any,
    ) -> int:
        """
        Prepares and runs the command inside a container.

        See Container.run() for more information.
        """

        return super().run(
            image, container_cmd, user, tempdir, env, datadir, script, **kwargs
        )

    def shell(
        self,
        image: str,
        user: Union[int, str],
        tempdir: Path,
        env: Optional[List[str]] = None,
        datadir: Optional[Path] = None,
        script: Optional[Path] = None,
        **kwargs: Any,
    ) -> int:
        """
        Spawns an interactive shell inside the container.

        See Container.shell() for more information
        """

        return super().shell(image, user, tempdir, env, datadir, script, **kwargs)

    def _images(self) -> Any:
        """
        Get all container images.

        :returns: a list of image details
        """

        img = super()._images()

        # Docker lacks proper JSON format output and instead of a single JSON
        # list object of all images it will return individual JSON objects
        # for all images, one per line
        images = [json.loads(image) for image in img.strip().split("\n") if image]

        log.debug(f"Deserialized {self.engine} images\n%s", images)
        return images

    def image_exists(self, image_ref: str, image_tag: str) -> bool:
        """
        Check if image exists in docker.
        :param image_ref: name/id/registry-path of image to check (str).
        :param image_tag: image tag (str).

        :returns: boolean
        """

        for img in self._images():
            id = img.get("ID")
            img_repository = img.get("Repository")
            img_tag = img.get("Tag", "latest")

            if (image_ref and id.startswith(image_ref)) or (
                image_ref == img_repository and image_tag == img_tag
            ):
                return True

        return False
