"""Exercise the real package implementation, including hostile inputs."""
import unittest
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.exceptions import InvalidSignature
from release_package import package, verify

class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.key=ec.generate_private_key(ec.SECP256R1())
        self.image=b'\xe9'+bytes(31)+b'\x32\x54\xcd\xab'+bytes(252)+b'SPKAPP:1:4.0.0:END'+bytes(range(256))*32
        self.data=package(self.image,'wroom','4.0.0',self.key)
    def test_roundtrip(self):
        self.assertEqual(verify(self.data,'wroom',self.key.public_key())['bytes'],len(self.image))
    def test_cross_target(self):
        with self.assertRaises(ValueError): verify(self.data,'wrover',self.key.public_key())
    def test_truncation_and_trailing(self):
        for data in (self.data[:50],self.data[:-1],self.data+b'junk'):
            with self.assertRaises((ValueError,InvalidSignature)): verify(data,'wroom',self.key.public_key())
    def test_payload_tamper(self):
        data=bytearray(self.data); data[-1]^=1
        with self.assertRaises(ValueError): verify(bytes(data),'wroom',self.key.public_key())
    def test_metadata_tamper(self):
        data=bytearray(self.data);data[24]^=1
        with self.assertRaises(InvalidSignature): verify(bytes(data),'wroom',self.key.public_key())
    def test_wrong_key(self):
        wrong=ec.generate_private_key(ec.SECP256R1()).public_key()
        with self.assertRaises(InvalidSignature): verify(self.data,'wroom',wrong)
    def test_reject_non_application(self):
        with self.assertRaises(ValueError): package(b'not firmware','wroom','4.0.0',self.key)
    def test_reject_wrong_image_identity(self):
        with self.assertRaises(ValueError): package(self.image,'wrover','4.0.0',self.key)
        with self.assertRaises(ValueError): package(self.image,'wroom','4.0.1',self.key)
    def test_reject_factory_image(self):
        factory = self.image[:32] + bytes(4) + self.image[36:]
        with self.assertRaises(ValueError): package(factory,'wroom','4.0.0',self.key)

if __name__=='__main__': unittest.main()
