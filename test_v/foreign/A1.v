
module A1
  (input din,
   output dout
   );
   // verilator foreign_module

   wire   tmp;
   A2 u_b(.din(din),
	 .dout(tmp));
   assign dout = ~tmp;
   
endmodule
