FROM node:8.9.1 AS build-env-node
ADD . /user_code
WORKDIR /user_code
RUN npm install --save local_modules/execer

FROM microsoft/dotnet:2.0.0-sdk AS build-env-dotnet
ADD . /user_code
WORKDIR /user_code
RUN dotnet restore -r ubuntu.14.04-x64
RUN dotnet publish -c Release -r ubuntu.14.04-x64 -o bin/
RUN chmod u+x bin/mainapp
RUN mkdir -p /user_code/lib && \
    for i in `dpkg -L libc6 libcurl3 libgcc1 libgssapi-krb5-2 libicu57 liblttng-ust0 libssl1.0.2 libstdc++6 libunwind8 libuuid1 zlib1g  | egrep "^/usr/lib/x86_64-linux-gnu/.*\.so\."`; do cp $i /user_code/lib; done

FROM gcr.io/distroless/dotnet
WORKDIR /user_code
ADD . /user_code
COPY --from=build-env-node /user_code/node_modules /user_code/node_modules
COPY --from=build-env-dotnet /user_code/lib /user_code/lib
COPY --from=build-env-dotnet /user_code/bin /user_code/bin

ENV ASPNETCORE_URLS "http://0.0.0.0:8080"
EXPOSE 8080
ENTRYPOINT ["bin/mainapp"]
