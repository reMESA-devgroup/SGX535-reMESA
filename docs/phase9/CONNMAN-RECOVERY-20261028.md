# Mini12 ConnMan supervision repair

The user requested repair of ConnMan repeatedly leaving the machine
unreachable. Physical access returned on stock boot
`7055b53d-aa35-427f-8239-aa96a71713f1`. The repair changed only network-service
configuration; it made no GPU submission or machine reboot.

Before repair, connmand1.44 was running as PID5372/PPID1 outside runit
supervision, while the existing runit service was down and its logger was
disabled. The saved Wi-Fi profile already had Favorite/AutoConnect enabled.
The old service exits170 if its initial D-Bus readiness check fails;
finish-default responds by requesting the service down. That is a verified
failure path, not proof of the exact original boot failure: logging was off.

Evidence: `/home/gama/sgx535-offline/connman-recovery-20261028/`, especially
SERVICE_BEFORE.json, LOGGING_BEFORE.json, STARTED.json, VERIFIED_RESTART.json,
STABILITY.json, CLOSEOUT.json and the authored run.proposed/worker.py.

Changes on the Mini12:

- /etc/sv/connman/run waits/retries for supervised D-Bus readiness and executes
  connmand in the foreground. A temporary dependency delay no longer takes the
  service permanently down through exit170. The dangling resolv.conf cleanup
  targets only a dangling symlink.
- /etc/runit/override-sysv.d/connman.block explicitly leaves startup ownership
  with the runit service. The SysV script and its runlevel links are preserved.
- The logger's down marker was removed after backup; bounded svlogd rotation
  is enabled with256KiB segments and four rotated files.

No Wi-Fi credentials, saved autoconnect preferences, kernel/module/GRUB files
or SGX artifacts were edited. D-Bus and wpa_supplicant were not restarted.

The detached repair worker had an automatic rollback if either fresh SSH
acceptance marker failed to arrive. Migration succeeded as PID31190 under
runsv1323, with online state and a verified new SSH connection. A controlled
SIGTERM then tested the supervisor: it automatically started connmand PID1017
under the same runsv, reconnected Wi-Fi at192.168.18.90, and a second new SSH
connection verified the result. Four later SSH samples over60seconds observed
the same supervised PID, online state and working logger. The worker is
terminal; there is no pending rollback action or permanent test watchdog.
This is a supervised restart/reconnection result, not a full-machine cold-boot
qualification or a guarantee against every future wireless failure.

Root-only backup/rollback on the Mini12:
`/root/connman-recovery-20261028T070528Z/`.
PLAN.json binds original files/modes/hashes. Manual rollback is available as:

```
sudo python3 /root/connman-recovery-20261028T070528Z/rollback.py --restore-original
```

It refuses an unexpected subsequently edited main run script. Rollback was
not executed after acceptance. Logs and all repair evidence remain preserved.
The next normal hardware-test boot should also check automatic startup; no
extra boot was imposed solely for this repair.

## SGX recovery verification

The independent read-only v4 WAIT_RECOVERY verifier now passes against the
same stock boot: exact stock module note484f9096..., observer absent, no
experimental controls/proc interface, original custom.cfg/saved default and
empty next_entry. Evidence is
`/home/gama/sgx535-offline/twod-fault-stage-20261028/v4/hardware/FINAL_REMOTE_STATE.json`.
This ends the earlier quarantined kernel lifetime by reset, not GPU retirement.
No experimental workload remains resident from the failed v4 boot. General
3D/2D/userspace/installable driver work remains active and unapproved as a whole.

## Subsequent full-boot verification

The next isolated driver boot, `8897d270-7f0d-491e-8cd8-c82b4fe2109d`, returned
online automatically. A fresh SSH inspection proved exactly one connmand,
PID1652 parent1473/runsv, with its logger running. Evidence is retained at
`/home/gama/sgx535-offline/twod-vendor-stream-20261028/v1/hardware/CONNMAN_COLD_BOOT.stdout`.
This supplies the full-machine startup check left pending by the original
service-only repair. No additional network configuration change was required.
