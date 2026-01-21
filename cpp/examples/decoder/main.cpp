/*
 * Copyright 2013-2022 Step Function I/O, LLC
 *
 * Licensed to Green Energy Corp (www.greenenergycorp.com) and Step Function I/O
 * LLC (https://stepfunc.io) under one or more contributor license agreements.
 * See the NOTICE file distributed with this work for additional information
 * regarding copyright ownership. Green Energy Corp and Step Function I/O LLC license
 * this file to you under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You may obtain
 * a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <opendnp3/ConsoleLogger.h>
#include <opendnp3/decoder/Decoder.h>
#include <opendnp3/logging/LogLevels.h>

#include <iostream>
#include <ostream>
#include <string>
#include <algorithm>
#include <vector>

using namespace opendnp3;

enum class Mode
{
    Link,
    Transport,
    App
};

Mode GetMode(const std::string& mode)
{
    if (mode == "link")
    {
        return Mode::Link;
    }
    else if (mode == "transport")
    {
        return Mode::Transport;
    }
    else
    {
        return Mode::App;
    }
}

int main(int argc, char* argv[])
{
    Logger logger(ConsoleLogger::Create(), ModuleId(), "decoder", LogLevels::everything());
    IDecoderCallbacks callback;
    Decoder decoder(callback, logger);

    const Mode MODE = (argc > 1) ? GetMode(argv[1]) : Mode::Link;

    std::string input, line;
    while (std::getline(std::cin, line))
    {
        // Append the line and a newline character to preserve the original formatting
        input += line + "\n";
    }
    input.erase(std::remove_if(input.begin(), input.end(), [](unsigned char x) { return std::isspace(x); }),
                input.end());
    if (input.length() % 2 != 0)
    {
        std::cerr << "Input length should be a factor of 2" << std::endl;
        return 1;
    }

    std::vector<uint8_t> bytes;
    for (auto it = input.begin(); it != input.end();)
    {
        auto abyte = std::string(it, it + 2);
        bytes.push_back(std::stoi(abyte, nullptr, 16));
        std::advance(it, 2);
    }

    switch (MODE)
    {
    case (Mode::Link):
        decoder.DecodeLPDU(Buffer(bytes.data(), bytes.size()));
        break;
    case (Mode::Transport):
        decoder.DecodeTPDU(Buffer(bytes.data(), bytes.size()));
        break;
    default:
        decoder.DecodeAPDU(Buffer(bytes.data(), bytes.size()));
        break;
    }

    return 0;
}
