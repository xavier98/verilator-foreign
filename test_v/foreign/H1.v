module H1
  (input clk,
   input din,
   output dout
   );
   // verilator foreign_module
   wire  tmp;
   H2 u_h2
     (.clk(clk),
      .din(din),
      .dout(tmp)
      );
   H3 u_h3
     (.clk(clk),
      .din(tmp),
      .dout(dout)
      );
endmodule

   
