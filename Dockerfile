FROM i386/centos:latest

RUN yum group install -y "Development Tools"

CMD ["bash"]