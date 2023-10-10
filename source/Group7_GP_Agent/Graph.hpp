
#pragma once

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../core/AgentBase.hpp"
#include "GraphNode.hpp"

namespace cowboys {
    using GraphLayer = std::vector<std::shared_ptr<GraphNode>>;

    class Graph {
    protected:
        /// Layers of nodes in the graph.
        std::vector<GraphLayer> layers;

    public:
        Graph() = default;
        ~Graph() = default;

        size_t GetNodeCount() const {
            size_t count = 0;
            for (const auto &layer : layers) {
                count += layer.size();
            }
            return count;
        }

        size_t GetLayerCount() const { return layers.size(); }

        /// @brief Makes a decision based on the inputs and the action vector.
        /// @param inputs The inputs to the graph.
        /// @param action_vec The action vector.
        /// @return The action to take.
        size_t MakeDecision(const std::vector<double> &inputs, const std::vector<size_t> &actions) {
            if (layers.size() == 0)
                return actions.at(0);

            // Set inputs of input layer
            size_t i = 0;
            for (auto &node : layers[0]) {
                double input = 0;
                if (i < inputs.size())
                    input = inputs.at(i);
                node->SetOutput(input);
                ++i;
            }

            // Get output of last layer
            std::vector<double> outputs;
            for (auto &node : layers.back()) {
                outputs.push_back(node->GetOutput());
            }

            std::cout << "Outputs: ";
            for (auto &output : outputs) {
                std::cout << output << " ";
            }
            std::cout << std::endl;

            // Choose the action with the highest output
            auto max_output = std::max_element(outputs.begin(), outputs.end());
            size_t index = std::distance(outputs.begin(), max_output);

            // If index is out of bounds, return the last action
            size_t action = 0;
            if (index >= actions.size())
                action = actions.back();
            else
                action = actions.at(index);
            std::cout << "Action: " << action << std::endl;
            return action;
        }

        void AddLayer(const GraphLayer &layer) { layers.push_back(layer); }
    };

    /// @brief Encodes the actions from an agent's action map into a vector of
    /// size_t, representing action IDs.
    /// @param action_map The action map from the agent.
    /// @return A vector of size_t, representing action IDs.
    std::vector<size_t> EncodeActions(const std::unordered_map<std::string, size_t> &action_map) {
        std::vector<size_t> actions;
        for (const auto &[action_name, action_id] : action_map) {
            actions.push_back(action_id);
        }
        // Sort the actions so that they are in a consistent order.
        std::sort(actions.begin(), actions.end());
        return actions;
    }

    /// @brief Translates state into nodes for the decision graph.
    /// @return A vector of doubles for the decision graph.
    std::vector<double> EncodeState(const cse491::WorldGrid &grid, const cse491::type_options_t & /*type_options*/,
                                    const cse491::item_set_t & /*item_set*/, const cse491::agent_set_t & /*agent_set*/,
                                    const cse491::Entity *agent,
                                    const std::unordered_map<std::string, double> &extra_agent_state) {
        /// TODO: Implement this function properly.
        std::vector<double> inputs;

        auto current_position = agent->GetPosition();

        double current_state = grid.At(current_position);
        double above_state = grid.IsValid(current_position.Above()) ? grid.At(current_position.Above()) : 0.;
        double below_state = grid.IsValid(current_position.Below()) ? grid.At(current_position.Below()) : 0.;
        double left_state = grid.IsValid(current_position.ToLeft()) ? grid.At(current_position.ToLeft()) : 0.;
        double right_state = grid.IsValid(current_position.ToRight()) ? grid.At(current_position.ToRight()) : 0.;

        double prev_action = extra_agent_state.at("previous_action");

        inputs.insert(inputs.end(), {prev_action, current_state, above_state, below_state, left_state, right_state});

        return inputs;
    }

} // namespace cowboys
