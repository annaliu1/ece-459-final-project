import * as THREE from "three";
import { GLTFLoader } from "three/examples/jsm/loaders/GLTFLoader.js";

let model; // we’ll store the loaded model here
let maskModel;

// Scene
const scene = new THREE.Scene();

// const container = document.getElementById("head-visualization");
// container.appendChild(renderer.domElement);

// Camera
const camera = new THREE.PerspectiveCamera(
  75,
  window.innerWidth / window.innerHeight,
  // container.clientWidth / container.clientHeight,
  0.1,
  1000
);

camera.position.z = 5;

// Renderer
const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setSize(window.innerWidth, window.innerHeight);
document.body.appendChild(renderer.domElement);
// renderer.setSize(container.clientWidth, container.clientHeight);

// Ambient light (overall brightness)
const ambientLight = new THREE.AmbientLight(0xffffff, 0.5);
scene.add(ambientLight);

// Directional light (like the sun)
const dirLight = new THREE.DirectionalLight(0xffffff, 1);
dirLight.position.set(5, 10, 7.5);
scene.add(dirLight);

// Point light (emits light in all directions, like a bulb)
const pointLight = new THREE.PointLight(0xffffff, 1);
pointLight.position.set(0, 5, 5);
scene.add(pointLight);

// Load GLB model
const loader = new GLTFLoader();
// loader.load(
//   "mask.glb", // <-- your file path
//   (gltf) => {
//     model = gltf.scene;
//     model.scale.set(0.2, 0.2, 0.2);
//     model.rotation.y -= 0.05;

//     const box = new THREE.Box3().setFromObject(model);
//     const center = box.getCenter(new THREE.Vector3());

//     model.position.x += (model.position.x - center.x);
//     model.position.y += (model.position.y - center.y);
//     model.position.z += (model.position.z - center.z);

//     scene.add(model);
//   },
//   (xhr) => {
//     console.log((xhr.loaded / xhr.total) * 100 + "% loaded");
//   },
//   (error) => {
//     console.error("Error loading model:", error);
//   }
// );

loader.load("head.glb", (gltf) => {
  model = gltf.scene;

  const box = new THREE.Box3().setFromObject(model);
  const center = box.getCenter(new THREE.Vector3());
  model.position.sub(center);
  model.rotation.x = -Math.PI / 2;
  model.scale.set(0.2, 0.2, 0.2);

  scene.add(model);

  // Once head loads, load mask
  // loader.load("mask.glb", (maskGltf) => {
  //   maskModel = maskGltf.scene;

  //   const maskBox = new THREE.Box3().setFromObject(maskModel);
  //   const maskCenter = maskBox.getCenter(new THREE.Vector3());
  //   maskModel.position.sub(maskCenter);
  //   model.scale.set(0.2, 0.2, 0.2);

  //   // Put the mask ON the head
  //   model.add(maskModel);
  // });
});

// Animation loop
function animate() {
  requestAnimationFrame(animate);

  if (model) {
    model.rotation.y += 0.01; // rotate around Y axis
    // model.rotation.x += 0.005; // rotate around X axis (optional)
    // model.rotation.z += 0.01;
  }

  renderer.render(scene, camera);
}
animate();
