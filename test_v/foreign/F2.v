module F2
  (input clk,
   input  din,
   output dout
   );
   // verilator foreign_module
   F3 u_c
     (.clk(clk),
      .din(din),
      .dout(dout)
      );
endmodule
