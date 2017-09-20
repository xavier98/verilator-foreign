
module E1
  (input clk,
   input rst,
   input  din,
   output dout
   );
   // verilator foreign_module
   wire   tmp;
   E2 u_b(.clk(clk),
	  .rst(rst),
	  .din(din),
	  .dout(tmp));
   assign dout = ~tmp;
endmodule
