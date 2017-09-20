module H2
  (input clk,
   input  din,
   output dout
   );
   // verilator foreign_module
   H2b u_h2b
     (.clk(clk),
      .din(din),
      .dout(dout)
      );
endmodule
