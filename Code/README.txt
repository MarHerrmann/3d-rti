README for 3D Radio Tomographic Imaging code:

- adjust project-conf.h in order to run simulations in cooja or deploy the application on real devices


NODE_ID	x
- node with ID 1 is root (compile rsroot.c)
- other nodes have IDs higher than 1 (compile rs.c)
- set the ID only if you want to deploy the application on real devices (max ID == NODE_AMOUNT)
- don't use ID 0


NODE_AMOUNT x 	(at least 8)
COOJA 0 or 1	(set it to 1 only if you want to run a simulation!)
START_SWITCH x	(start changing channels - value modulo CHANNELS should be zero)
ITERATIONS x	(one second has approx. 3 iterations)
NODE_DELAY x	(spread out the transmission timings - slower timings)