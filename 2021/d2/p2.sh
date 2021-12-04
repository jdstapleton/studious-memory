#!/usr/bin/env scala  -deprecation
import scala.io.Source
import java.io.Closeable

object Directions extends Enumeration {
  type Direction = Value

  val up, forward, down = Value
}

case class Vector(dir: Directions.Direction, magnitude: Int) {  }

class Sub(var x: Int, var depth: Int, aim: Int) {
  override def toString = s"$x, $depth, $aim, ${x * depth}"
  def move(movement: Vector): Sub = movement match {
  	case Vector(Directions.up, magnitude) => new Sub(x, depth, aim - magnitude)
  	case Vector(Directions.down, magnitude) => new Sub(x, depth, aim + magnitude)
  	case Vector(Directions.forward, magnitude) => new Sub(x + magnitude, depth + (aim * magnitude), aim)
  	case Vector(_, _) => this
  }
}

object Main {
	def parsedVector(txt: String): Vector = txt match { case s"$dir $magnitude" => Vector(Directions.withName(dir), Integer.parseInt(magnitude)) }

	def cleanly[A<:Closeable,B](resource: => A)(code: A => B): Either[Exception,B] = {
      try {
        val r = resource
        try { Right(code(r)) } finally { r.close() }
      }
      catch { case e: Exception => Left(e) }
    }

    def main(args: Array[String]): Unit = {
    	cleanly(Source.fromFile(args(0))) { fin =>
    	  fin.getLines()
    	      .map(parsedVector)
    	      .foldLeft(new Sub(0, 0, 0))((a: Sub, c: Vector) => a.move(c))
		} match {
		  case Right(sub: Sub) => println(s"Success $sub")
		  case Left(ex: Exception) => println(s"Error $ex")
		}
    }
}