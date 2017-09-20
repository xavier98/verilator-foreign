module D2
  (input clk,
   input din,
   output dout
   );
   // verilator foreign_module
   reg   doutr;
   assign dout = doutr;
   wire  tmp = ~din;
   wire  clk2 = tmp ^ clk;
   always @(posedge clk2)
     doutr <= din;
endmodule
