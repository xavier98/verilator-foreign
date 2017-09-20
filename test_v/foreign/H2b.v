module H2b
  (input clk,
   input  din,
   output dout
   );
   // verilator foreign_module
   reg 	  doutr;
   always @(posedge clk) doutr <= din;
   assign dout = doutr;
endmodule
