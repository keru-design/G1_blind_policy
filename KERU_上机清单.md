# G1 板载部署 —— 上机清单（Keru）

policy：`deploy/robots/g1/config/policy/velocity/v0/`（model_19999，98 维观测）
仓库：https://github.com/keru-design/G1_blind_policy

---

## 0. 出发前（笔记本上，5 分钟）

- [ ] 仿真再跑一遍，确认能走（先开终端 2 再开终端 1）：
  ```bash
  cd ~/unitree_G1/blind_policy/unitree_rl_mjlab/deploy/robots/g1/build && ./g1_ctrl --network=lo --auto_start --keyboard
  cd ~/unitree_G1/blind_policy/unitree_rl_mjlab && ./simulate/build/unitree_mujoco
  ```
- [ ] 带上：网线、G1 遥控器（充好电）、吊架

## 1. 先问 Hari（不知道就别往下做）

- [ ] 板载电脑 IP（通常 `192.168.123.164`）、账号、密码
- [ ] 板载电脑能否上网（决定第 4 步用 git clone 还是从笔记本拷）
- [ ] 板载电脑上是否已装 unitree_sdk2

## 2. 机器人上电（README 4.1 / 4.2）

- [ ] G1 **吊在吊架上**再开机
- [ ] 等进入零力矩模式（关节软）
- [ ] 遥控器按 `L2 + R2` → 调试模式（关节有阻尼）

## 3. 笔记本连上机器人

- [ ] 网线：笔记本网口 ↔ G1
- [ ] 笔记本网口设 IP（只需第一次）：
  ```bash
  nmcli con add type ethernet ifname eno2 con-name g1 ipv4.method manual ipv4.addresses 192.168.123.222/24 && nmcli con up g1
  ```
- [ ] 测试：`ping 192.168.123.164` 有回复
- [ ] 登录板载电脑：`ssh 账号@192.168.123.164`

> 以下第 4–7 步都在**板载电脑**上执行（ssh 登录后）。

## 4. 拿代码

能上网：
```bash
cd ~ && git clone https://github.com/keru-design/G1_blind_policy.git
```
（私有仓库需要登录 GitHub，现场再弄）

不能上网 —— 在**笔记本**上执行，把代码拷过去：
```bash
cd ~/unitree_G1/blind_policy && git -C unitree_rl_mjlab archive --format=tar.gz -o /tmp/g1.tar.gz HEAD && scp /tmp/g1.tar.gz 账号@192.168.123.164:~/ && ssh 账号@192.168.123.164 'mkdir -p ~/G1_blind_policy && tar xzf ~/g1.tar.gz -C ~/G1_blind_policy'
```

## 5. 装依赖（板载电脑上，只需第一次）

- [ ] 检查 SDK：`ls /usr/local/lib/cmake/unitree_sdk2 /opt/unitree_robotics 2>/dev/null`
- [ ] 系统库：
  ```bash
  sudo apt update && sudo apt install -y libyaml-cpp-dev libboost-all-dev libeigen3-dev libspdlog-dev libfmt-dev
  ```
- [ ] 没有 SDK 的话：
  ```bash
  cd ~ && git clone https://github.com/unitreerobotics/unitree_sdk2.git && cd unitree_sdk2 && mkdir build && cd build && cmake .. && make -j4 && sudo make install
  ```

## 6. 编译（板载电脑上）

```bash
cd ~/G1_blind_policy/deploy/robots/g1 && mkdir -p build && cd build && cmake .. && make -j4
```
- [ ] 看到 `Built target g1_ctrl` 即成功（ARM 版 onnxruntime 会自动选用）
- [ ] 查板载电脑连电机的网口名：`ip -br addr | grep 192.168.123` → 通常是 `eth0`

## 7. 运行（机器人仍吊着）

```bash
cd ~/G1_blind_policy/deploy/robots/g1/build && ./g1_ctrl --network=eth0
```
**绝对不要加 `--auto_start` 或 `--keyboard`**

| 操作 | 遥控器 | 结果 |
|---|---|---|
| 站起来 | `LT + ↑` | 吊着状态下站直 |
| 放下 | — | 慢慢降吊架让脚着地，吊绳先别全松 |
| 开始 RL | `RT + A` | policy 接管，站稳后再松吊绳 |
| 走 | 左摇杆前后左右，右摇杆转向 | 先轻推，最快 1.0 m/s |
| **急停** | `LT + B` | 阻尼模式，机器人软下来 |

终端里按 `0` 也能切阻尼（第二个急停）。

## 8. 出问题时

| 现象 | 处理 |
|---|---|
| 卡在 `Waiting for connection` | 网口名不对 / 没进调试模式（重按 `L2+R2`） |
| `The other process is using the lowcmd channel` | 宇树自带运动程序还在跑，没进调试模式 |
| 切 RL 后抖动 / 歪 | 立刻 `LT + B`，记下现象，别重复试 |
| 其他报错 | 把终端输出整段拍照 / 复制下来 |
