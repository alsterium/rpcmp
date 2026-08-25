# Core Runtime

Platform-neutral library, player, engine, scheduler, device-port, and snapshot implementation. This module must not import or link `core/ui`.

M0 provides `rpcmp::runtime::MockCore`, an injected-clock, bounded-command reference runtime. It uses only public contracts and fake observer/device ports; it contains no MDX parser, real library reader, audio backend, or UI dependency.
