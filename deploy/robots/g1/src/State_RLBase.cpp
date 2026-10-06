#include "FSM/State_RLBase.h"
#include "unitree_articulation.h"
#include "isaaclab/envs/mdp/observations/observations.h"
#include "isaaclab/envs/mdp/actions/joint_actions.h"
#include <unordered_map>

namespace isaaclab
{
// keyboard velocity commands example
// change "velocity_commands" observation name in policy deploy.yaml to "keyboard_velocity_commands"
REGISTER_OBSERVATION(keyboard_velocity_commands)
{
    std::string key = FSMState::keyboard->key();
    static auto cfg = env->cfg["commands"]["base_velocity"]["ranges"];

    static std::unordered_map<std::string, std::vector<float>> key_commands = {
        {"w", {1.0f, 0.0f, 0.0f}},
        {"s", {-1.0f, 0.0f, 0.0f}},
        {"a", {0.0f, 1.0f, 0.0f}},
        {"d", {0.0f, -1.0f, 0.0f}},
        {"q", {0.0f, 0.0f, 1.0f}},
        {"e", {0.0f, 0.0f, -1.0f}}
    };
    std::vector<float> cmd = {0.0f, 0.0f, 0.0f};
    if (key_commands.find(key) != key_commands.end())
    {
        cmd = key_commands[key];
    }
    return cmd;
}

// Keyboard velocity commands with memory (Keru):
//   w/s: vx +/-0.1   a/d: vy +/-0.1   q/e: wz +/-0.1   space: stop
// Each key press changes the command once; the command is kept until changed.
inline std::vector<float> keyboard_sticky_cmd = {0.0f, 0.0f, 0.0f};

REGISTER_OBSERVATION(keyboard_sticky_velocity_commands)
{
    static std::string last_key = "";
    std::string key = FSMState::keyboard->key();
    auto ranges = env->cfg["commands"]["base_velocity"]["ranges"];
    if(key != last_key && key != "")
    {
        auto & c = keyboard_sticky_cmd;
        if(key == "w") c[0] += 0.1f;
        else if(key == "s") c[0] -= 0.1f;
        else if(key == "a") c[1] += 0.1f;
        else if(key == "d") c[1] -= 0.1f;
        else if(key == "q") c[2] += 0.1f;
        else if(key == "e") c[2] -= 0.1f;
        else if(key == " ") c = {0.0f, 0.0f, 0.0f};
        c[0] = std::clamp(c[0], ranges["lin_vel_x"][0].as<float>(), ranges["lin_vel_x"][1].as<float>());
        c[1] = std::clamp(c[1], ranges["lin_vel_y"][0].as<float>(), ranges["lin_vel_y"][1].as<float>());
        c[2] = std::clamp(c[2], ranges["ang_vel_z"][0].as<float>(), ranges["ang_vel_z"][1].as<float>());
        spdlog::info("keyboard cmd: vx={:.1f} vy={:.1f} wz={:.1f}", c[0], c[1], c[2]);
    }
    last_key = key;
    return keyboard_sticky_cmd;
}

// Same as gait_phase, but gated by the keyboard command instead of the joystick.
REGISTER_OBSERVATION(keyboard_gait_phase)
{
    float period = params["period"].as<float>();
    env->global_phase = std::fmod(env->global_phase + env->step_dt / period, 1.0f);
    auto & c = keyboard_sticky_cmd;
    float cmd_norm = std::sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
    if(cmd_norm < 0.1f) return {0.0f, 0.0f};
    return {std::sin(env->global_phase * 2 * (float)M_PI), std::cos(env->global_phase * 2 * (float)M_PI)};
}

}

State_RLBase::State_RLBase(int state_mode, std::string state_string)
: FSMState(state_mode, state_string) 
{
    auto cfg = param::config["FSM"][state_string];
    auto policy_dir = param::parser_policy_dir(cfg["policy_dir"].as<std::string>());

    // config.yaml can pick another file, e.g. deploy_file: deploy_keyboard.yaml
    auto deploy_file = cfg["deploy_file"] ? cfg["deploy_file"].as<std::string>() : std::string("deploy.yaml");
    if(FSMState::keyboard_cmd) deploy_file = "deploy_keyboard.yaml";
    spdlog::info("Loading {}", (policy_dir / "params" / deploy_file).string());
    env = std::make_unique<isaaclab::ManagerBasedRLEnv>(
        YAML::LoadFile(policy_dir / "params" / deploy_file),
        std::make_shared<unitree::BaseArticulation<LowState_t::SharedPtr>>(FSMState::lowstate)
    );
    env->alg = std::make_unique<isaaclab::OrtRunner>(policy_dir / "exported" / "policy.onnx");

    this->registered_checks.emplace_back(
        std::make_pair(
            [&]()->bool{ return isaaclab::mdp::bad_orientation(env.get(), 1.0); },
            FSMStringMap.right.at("Passive")
        )
    );
}

void State_RLBase::run()
{
    auto action = env->action_manager->processed_actions();
    for(int i(0); i < env->robot->data.joint_ids_map.size(); i++) {
        lowcmd->msg_.motor_cmd()[env->robot->data.joint_ids_map[i]].q() = action[i];
    }
}