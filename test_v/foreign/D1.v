
module D1
  (input clk,
   input  din,
   output dout
   );
   // verilator foreign_module
   wire   tmp;
   D2 u_b(.clk(clk),
	  .din(din),
	  .dout(tmp));
   assign dout = ~tmp;
endmodule
