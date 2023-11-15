/**
 * This file is part of the Fall 2023, CSE 491 course project.
 * @brief A networking interface that allows information to be sent across a  network
 * @note Status: PROTOTYPE
 **/

#pragma once
#include <thread>
#include <map>
#include <sstream>
#include <vector>
#include "Interfaces/NetWorth/NetworkInterface.hpp"

namespace netWorth{
    using namespace sf;

    /**
     * The server that will be running and that allows clients to connect to
     */
    class ServerManager {
    private:
        std::vector<std::thread> m_clientThreads; ///Vector of all client threads
        std::map<unsigned short, size_t> m_action_map; ///Map of port numbers to most recent action selected

    protected:

    public:
        const static constexpr unsigned short m_initConnectionPort = 55000; ///Port for initial client connection
        unsigned short m_maxClientPort = 55000; ///Port that is incremented for client thread handoff
        /**
         * Default constructor (AgentBase)
         * @param id agent ID
         * @param name agent name
         */
        ServerManager()= default;

        /**
         * Increases the max client port
         */
        void IncreasePort(){++m_maxClientPort;}

        /**
         * Adds a thread to the container of threads for cleanup and reference purposes
         * @param thread thread to add into the vector
         */
        void AddClient(std::thread &thread){m_clientThreads.push_back(std::move(thread));}

        /**
         * Joins all client threads at the end of the server's lifespan
         */
        void JoinClients(){
            for (auto &thread: m_clientThreads){
                thread.join();
            }
        }

    }; // End of class ServerManager
} // End of namespace netWorth