# NOTES:
# 1. The Dockerfile does not have an entry point because everything has to be managed outside of it.
# 2. Please do not install anything directly in this file.
#   You need to use "install_deps.sh" - we want to keep a possibility to develop w/o docker.
# 3. If a tool or library that you install in the "install_deps.sh" is needed only for building 3rd party
#   tools/libraries please uninstall them after the script run - we want to keep image' size minimal.
#   For example wget and pip-tools.


FROM ubuntu:20.04
MAINTAINER pzs1997
LABEL Description="Build environment for neuchain"

ENV HOME /root
ADD ./ /root/neuchain
ARG DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-c"]
RUN echo $'path-exclude /usr/share/doc/* \n\
path-exclude /usr/share/doc/*/copyright \n\
path-exclude /usr/share/man/* \n\
path-exclude /usr/share/groff/* \n\
path-exclude /usr/share/info/* \n\
path-exclude /usr/share/lintian/* \n\
path-exclude /usr/share/linda/*' > /etc/dpkg/dpkg.cfg.d/01_nodoc && \
/bin/bash -c "chmod +x /root/neuchain/install_deps.sh" && \
/bin/bash -c "/root/neuchain/install_deps.sh" && \
apt-get clean && \
apt-get autoclean && \
rm -rf /var/lib/apt/lists/* && \
apt-get purge --auto-remove -y
# For eliot-tree's rendering
ENV LC_ALL=C.UTF-8
WORKDIR /root
# VOLUME /root/dd-chain
EXPOSE 22