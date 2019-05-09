<<<<<<< HEAD
FROM gymreklab/str-toolkit-2.4:latest
=======
FROM gymreklab/str-toolkit:latest
>>>>>>> 93385b86940a8ec2595b47e0147b9e1e8f33ab26

# Get necessary packages
RUN apt-get install -qqy \
  autotools-dev \
  automake \
<<<<<<< HEAD
  libtool libtool-bin \
  libgsl-dev

# Install packages
RUN pip3 install pycrypto
RUN pip3 install pandas==0.22.0

# Download, compile, and install GangSTR
RUN wget -O GangSTR-2.4.2.tar.gz https://github.com/gymreklab/GangSTR/releases/download/v2.4.2/GangSTR-2.4.2.tar.gz
RUN tar -xzvf GangSTR-2.4.2.tar.gz
WORKDIR GangSTR-2.4.2
RUN ./install-gangstr.sh
RUN ldconfig
WORKDIR ..

# Download, compile, and install Vcftools
RUN wget https://github.com/vcftools/vcftools/releases/download/v0.1.16/vcftools-0.1.16.tar.gz
RUN tar -xzvf vcftools-0.1.16.tar.gz
WORKDIR vcftools-0.1.16
RUN ./autogen.sh && ./configure && make && make install
WORKDIR ..

# Download, compile, and install CookieMonSTR
RUN git clone https://github.com/gymreklab/STRDenovoTools
WORKDIR STRDenovoTools
RUN ./reconf
=======
  libgsl-dev

# Download, compile, and install CookieMonSTR
RUN git clone https://github.com/gymreklab/STRDenovoTools
WORKDIR STRDenovoTools
RUN autoreconf --install
RUN autoconf
>>>>>>> 93385b86940a8ec2595b47e0147b9e1e8f33ab26
RUN ./configure
RUN make
RUN make install
WORKDIR ..
