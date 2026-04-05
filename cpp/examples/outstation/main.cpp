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
#include <opendnp3/DNP3Manager.h>
#include <opendnp3/channel/PrintingChannelListener.h>
#include <opendnp3/logging/LogLevels.h>
#include <opendnp3/outstation/DefaultOutstationApplication.h>
#include <opendnp3/outstation/IUpdateHandler.h>
#include <opendnp3/outstation/SimpleCommandHandler.h>
#include <opendnp3/outstation/UpdateBuilder.h>

#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace std;
using namespace opendnp3;

DatabaseConfig ConfigureDatabase(int point_count)
{
    DatabaseConfig config(point_count);

    config.analog_input[0].clazz = PointClass::Class2;
    config.analog_input[0].svariation = StaticAnalogVariation::Group30Var5;
    config.analog_input[0].evariation = EventAnalogVariation::Group32Var7;

    return config;
}

struct State
{
    uint32_t count = 0;
    double value = 0;
    bool binary = false;
    DoubleBit dbit = DoubleBit::DETERMINED_OFF;
    uint8_t octetStringValue = 1;
};

auto app = DefaultOutstationApplication::Create();

void AddUpdates(UpdateBuilder& builder, State& state, const std::string& arguments);

int main(int argc, char* argv[])
{
    if(argc != 4) {
      std::cout << "missing arguments: ./app port start_address rtu_count";
      return -1;
    }
    auto port = stoi(string(argv[1])); 
    auto start_address = stoi(string(argv[2])); 
    auto rtu_count = stoi(string(argv[3])); 
    // Specify what log levels to use. NORMAL is warning and above
    // You can add all the comms logging by uncommenting below.
    const auto logLevels = levels::NORMAL | levels::ALL_COMMS;

    // This is the main point of interaction with the stack
    // Allocate a single thread to the pool since this is a single outstation
    // Log messages to the console
    DNP3Manager manager(1, ConsoleLogger::Create());

    // Create a TCP server (listener)
    auto channel = std::shared_ptr<IChannel>(nullptr);
    try
    {
        channel = manager.AddTCPServer("server", logLevels, ServerAcceptMode::CloseExisting,
                                       IPEndpoint("0.0.0.0", port), PrintingChannelListener::Create());
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return -1;
    }
    auto point_count = 100;
    vector<std::shared_ptr<IOutstation>> outstations;
    for (auto i = start_address; i < start_address+rtu_count; i++)
    {
        // The main object for a outstation. The defaults are useable,
        // but understanding the options are important.
        OutstationStackConfig config(ConfigureDatabase(point_count));

        // Specify the maximum size of the event buffers
        config.outstation.eventBufferConfig = EventBufferConfig::AllTypes(100);

        // you can override an default outstation parameters here
        // in this example, we've enabled the oustation to use unsolicted reporting
        // if the master enables it
        config.outstation.params.allowUnsolicited = true;

        // You can override the default link layer settings here
        // in this example we've changed the default link layer addressing
        config.link.LocalAddr = i;
        config.link.RemoteAddr = 1;
        config.link.KeepAliveTimeout = TimeDuration::Max();

        // Create a new outstation with a log level, command handler, and
        // config info this	returns a thread-safe interface used for
        // updating the outstation's database.
        auto outstation = channel->AddOutstation("outstation", SuccessCommandHandler::Create(), app, config);

        // Enable the outstation and start communications
        outstation->Enable();
        UpdateBuilder builder;
        for (auto j = 0; j < point_count; j++)
        {
            builder.Update(Counter(i), j, EventMode::Suppress);
            builder.Update(Analog(i), j, EventMode::Suppress);
            builder.Update(AnalogOutputStatus(i), j, EventMode::Suppress);
            builder.Update(Binary(i % 2), j, EventMode::Suppress);
            builder.Update(BinaryOutputStatus(i % 2), j, EventMode::Suppress);
        }
        outstation->Apply(builder.Build());
        outstations.push_back(outstation);
    }

    // variables used in example loop
    string input;
    State state;

    while (true)
    {
        std::cout << "Enter one or more measurement changes then press <enter>" << std::endl;
        std::cout << "c = counter, b = binary, d = doublebit, a = analog, o = octet string, 'quit' = exit" << std::endl;
        std::cin >> input;

        if (input == "quit")
            return 0; // DNP3Manager destructor cleanups up everything automatically
        else
        {
            // update measurement values based on input string
            UpdateBuilder builder;
            AddUpdates(builder, state, input);
            // outstation->Apply(builder.Build());
        }
    }

    return 0;
}

void AddUpdates(UpdateBuilder& builder, State& state, const std::string& arguments)
{
    for (const char& c : arguments)
    {
        switch (c)
        {
        case ('c'): {
            builder.Update(Counter(state.count), 0);
            ++state.count;
            break;
        }
        case ('f'): {
            builder.FreezeCounter(0, false);
            break;
        }
        case ('a'): {
            builder.Update(Analog(state.value), 0);
            state.value += 1;
            break;
        }
        case ('b'): {
            builder.Update(Binary(state.binary, Flags(0x01), app->Now()), 0);
            state.binary = !state.binary;
            break;
        }
        case ('d'): {
            builder.Update(DoubleBitBinary(state.dbit), 0);
            state.dbit
                = (state.dbit == DoubleBit::DETERMINED_OFF) ? DoubleBit::DETERMINED_ON : DoubleBit::DETERMINED_OFF;
            break;
        }
        case ('o'): {
            OctetString value(Buffer(&state.octetStringValue, 1));
            builder.Update(value, 0);
            state.octetStringValue += 1;
            break;
        }
        default:
            break;
        }
    }
}
