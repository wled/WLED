FROM ubuntu:latest

RUN apt-get update \
    && apt-get install -y --no-install-recommends nodejs npm git ca-certificates python3-pip \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workdir

COPY . .

RUN npm ci
RUN pip install --break-system-packages --ignore-installed -r requirements.txt

CMD ["bash"]
